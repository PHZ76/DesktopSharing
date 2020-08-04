#include "QsvEncoder.h"
#include "common_utils.h"
#include "net/log.h"
#include <Windows.h>
#include <versionhelpers.h>

QsvEncoder::QsvEncoder()
{
	mfx_impl_ = MFX_IMPL_AUTO_ANY;
	mfx_ver_ = { {0, 1} };

	mfxIMPL impl = mfx_impl_;

	if(IsWindows8OrGreater()) {
		use_d3d11_ = true;
		impl = mfx_impl_ | MFX_IMPL_VIA_D3D11;
	}
	else {
		impl = mfx_impl_ | MFX_IMPL_VIA_D3D9;
		use_d3d9_ = true;
	}

	mfxStatus sts = MFX_ERR_NONE;
	sts = mfx_session_.Init(impl, &mfx_ver_);
	if (sts == MFX_ERR_NONE) {
		mfx_session_.QueryVersion(&mfx_ver_);
		mfx_session_.Close();
	}
}

QsvEncoder::~QsvEncoder()
{
	Destroy();
}

bool QsvEncoder::Init(QsvParams& qsv_params)
{
	mfxStatus sts = MFX_ERR_NONE;

	sts = Initialize(mfx_impl_, mfx_ver_, &mfx_session_, &mfx_allocator_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	if (!InitParams(qsv_params)) {
		return false;
	}

	mfx_encoder_.reset(new MFXVideoENCODE(mfx_session_));

	sts = mfx_encoder_->Query(&mfx_enc_params_, &mfx_enc_params_);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	sts = mfx_encoder_->Init(&mfx_enc_params_);
	if (sts != MFX_ERR_NONE) {
		mfx_encoder_.reset();
		return false;
	}

	if (!AllocateSurfaces()) {
		return false;
	}

	if (!AllocateBuffer()) {
		return false;
	}

	GetVideoParam();
	is_initialized_ = true;
	return true;
}

void QsvEncoder::Destroy()
{
	if (is_initialized_) {
		FreeSurface();
		Release();
		mfx_encoder_->Close();
		is_initialized_ = false;
	}
}

bool QsvEncoder::InitParams(QsvParams& qsv_params)
{
	memset(&mfx_enc_params_, 0, sizeof(mfx_enc_params_));

	if (qsv_params.codec == "h264") {
		mfx_enc_params_.mfx.CodecId = MFX_CODEC_AVC;
		mfx_enc_params_.mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;
	}
	else if (qsv_params.codec == "hevc") {
		mfx_enc_params_.mfx.CodecId = MFX_CODEC_HEVC;
		mfx_enc_params_.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
	}
	else {
		return false;
	}

	mfx_enc_params_.mfx.GopOptFlag = MFX_GOP_STRICT;
	mfx_enc_params_.mfx.NumSlice = 1;
	// MFX_TARGETUSAGE_BEST_SPEED; MFX_TARGETUSAGE_BALANCED
	mfx_enc_params_.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
	mfx_enc_params_.mfx.FrameInfo.FrameRateExtN = qsv_params.framerate;
	mfx_enc_params_.mfx.FrameInfo.FrameRateExtD = 1;
	mfx_enc_params_.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	mfx_enc_params_.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfx_enc_params_.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfx_enc_params_.mfx.FrameInfo.CropX = 0;
	mfx_enc_params_.mfx.FrameInfo.CropY = 0;
	mfx_enc_params_.mfx.FrameInfo.CropW = qsv_params.width;
	mfx_enc_params_.mfx.FrameInfo.CropH = qsv_params.height;

	mfx_enc_params_.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
	mfx_enc_params_.mfx.TargetKbps = qsv_params.bitrate_kbps;
	//mfx_enc_params_.mfx.MaxKbps = param.bitrate_kbps;

	mfx_enc_params_.mfx.GopPicSize = (mfxU16)qsv_params.gop;
	mfx_enc_params_.mfx.IdrInterval = (mfxU16)qsv_params.gop;

	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a
	// multiple of 32 in case of field picture
	mfx_enc_params_.mfx.FrameInfo.Width = MSDK_ALIGN16(qsv_params.width);
	mfx_enc_params_.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == mfx_enc_params_.mfx.FrameInfo.PicStruct) ?
		MSDK_ALIGN16(qsv_params.height) : MSDK_ALIGN32(qsv_params.height); //MSDK_ALIGN16(param.height);

	// d3d11 or d3d9
	mfx_enc_params_.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

	// Configuration for low latency
	mfx_enc_params_.AsyncDepth = 1;  //1 is best for low latency
	mfx_enc_params_.mfx.GopRefDist = 1; //1 is best for low latency, I and P frames only

	memset(&extended_coding_options_, 0, sizeof(mfxExtCodingOption));
	extended_coding_options_.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
	extended_coding_options_.Header.BufferSz = sizeof(mfxExtCodingOption);
	//option.RefPicMarkRep = MFX_CODINGOPTION_ON;
	extended_coding_options_.NalHrdConformance = MFX_CODINGOPTION_OFF;
	extended_coding_options_.PicTimingSEI = MFX_CODINGOPTION_OFF;
	extended_coding_options_.AUDelimiter = MFX_CODINGOPTION_OFF;
	extended_coding_options_.MaxDecFrameBuffering = 1;

	memset(&extended_coding_options2_, 0, sizeof(mfxExtCodingOption2));
	extended_coding_options2_.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
	extended_coding_options2_.Header.BufferSz = sizeof(mfxExtCodingOption2);
	extended_coding_options2_.RepeatPPS = MFX_CODINGOPTION_OFF;

	extended_buffers_[0] = (mfxExtBuffer*)(&extended_coding_options_);
	extended_buffers_[1] = (mfxExtBuffer*)(&extended_coding_options2_);
	mfx_enc_params_.ExtParam = extended_buffers_;
	mfx_enc_params_.NumExtParam = 2;

	return true;
}

bool QsvEncoder::AllocateSurfaces()
{
	mfxStatus sts = MFX_ERR_NONE;

	// Query number of required surfaces for encoder
	mfxFrameAllocRequest enc_request;
	memset(&enc_request, 0, sizeof(mfxFrameAllocRequest));
	sts = mfx_encoder_->QueryIOSurf(&mfx_enc_params_, &enc_request);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	// This line is only required for Windows DirectX11 to ensure that surfaces can be written to by the application
	enc_request.Type |= WILL_WRITE;

	// Allocate required surfaces
	sts = mfx_allocator_.Alloc(mfx_allocator_.pthis, &enc_request, &mfx_alloc_response_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	mfxU16 num_surfaces = mfx_alloc_response_.NumFrameActual;

	// Allocate surface headers (mfxFrameSurface1) for encoder
	mfx_surfaces_.resize(num_surfaces);
	for (int i = 0; i < num_surfaces; i++) {
		memset(&mfx_surfaces_[i], 0, sizeof(mfxFrameSurface1));
		mfx_surfaces_[i].Info = mfx_enc_params_.mfx.FrameInfo;
		mfx_surfaces_[i].Data.MemId = mfx_alloc_response_.mids[i];
		ClearYUVSurfaceVMem(mfx_surfaces_[i].Data.MemId);
	}

	return true;
}

void QsvEncoder::FreeSurface()
{
	if (mfx_alloc_response_.NumFrameActual > 0) {
		mfx_allocator_.Free(mfx_allocator_.pthis, &mfx_alloc_response_);
		memset(&mfx_alloc_response_, 0, sizeof(mfxFrameAllocResponse));
	}
}

bool QsvEncoder::AllocateBuffer()
{
	mfxStatus sts = MFX_ERR_NONE;
	mfxVideoParam param;
	memset(&param, 0, sizeof(mfxVideoParam));
	sts = mfx_encoder_->GetVideoParam(&param);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	memset(&mfx_enc_bs_, 0, sizeof(mfxBitstream));
	mfx_enc_bs_.MaxLength = param.mfx.BufferSizeInKB * 1000;
	bst_enc_data_.resize(mfx_enc_bs_.MaxLength);
	mfx_enc_bs_.Data = bst_enc_data_.data();
	return true;
}

void QsvEncoder::FreeBuffer()
{
	memset(&mfx_enc_bs_, 0, sizeof(mfxBitstream));
	bst_enc_data_.clear();
}

bool QsvEncoder::GetVideoParam()
{
	mfxExtCodingOptionSPSPPS opt;
	memset(&mfx_video_params_, 0, sizeof(mfxVideoParam));
	opt.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
	opt.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);

	static mfxExtBuffer *extendedBuffers[1];
	extendedBuffers[0] = (mfxExtBuffer *)&opt;
	mfx_video_params_.ExtParam = extendedBuffers;
	mfx_video_params_.NumExtParam = 1;

	opt.SPSBuffer = sps_buffer_.get();
	opt.PPSBuffer = pps_buffer_.get();
	opt.SPSBufSize = 1024;
	opt.PPSBufSize = 1024;

	mfxStatus sts = mfx_encoder_->GetVideoParam(&mfx_video_params_);
	if (sts != MFX_ERR_NONE) {
		return false;
	}

	sps_size_ = opt.SPSBufSize;
	pps_size_ = opt.PPSBufSize;
	return true;
}

void QsvEncoder::ForceIDR()
{
	if (is_initialized_) {
		enc_ctrl_.FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
	}
}

void QsvEncoder::SetBitrate(uint32_t bitrate_kbps)
{
	if (mfx_encoder_) {
		mfxVideoParam old_param;
		memset(&old_param, 0, sizeof(mfxVideoParam));
		mfxStatus status = mfx_encoder_->GetVideoParam(&old_param);
		MSDK_IGNORE_MFX_STS(status, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

		if (status == MFX_ERR_NONE) {
			uint32_t old_bitrate = old_param.mfx.TargetKbps;
			old_param.mfx.TargetKbps = bitrate_kbps;
			status = mfx_encoder_->Reset(&old_param);
			MSDK_IGNORE_MFX_STS(status, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
			if (status == MFX_ERR_NONE) {
				LOG("Reset bitrate:%u, old bitrate:%u", bitrate_kbps, old_bitrate);
			}
			else {
				LOG("[Reset bitrate failed, bitrate:%u, status:%d",
					old_param.mfx.TargetKbps, status);
			}
		}
		else {
			LOG("GetVideoParam() failed");
		}
	}
}