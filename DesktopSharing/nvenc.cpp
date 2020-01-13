#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "nvenc.h"
#include <cstdint>
#include <string>
#include <wrl.h>
#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_2.h>

struct nvenc_data
{
	Microsoft::WRL::ComPtr<ID3D11Device>        device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<IDXGIAdapter>        adapter;
	Microsoft::WRL::ComPtr<IDXGIFactory1>       factory;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>     texture;

	std::mutex mutex;
	uint32_t width;
	uint32_t height;
	uint32_t framerate;
	uint32_t bitrate;
	uint32_t gop;
	std::string codec;
	DXGI_FORMAT format;
	NvEncoderD3D11 *nvenc = nullptr;
};

static bool is_supported(void)
{
#if defined(_WIN64)
	HMODULE hModule = LoadLibrary(TEXT("nvEncodeAPI64.dll"));
#elif defined(_WIN32)
	HMODULE hModule = LoadLibrary(TEXT("nvEncodeAPI.dll"));
#endif

	if (hModule == NULL)
	{
		printf("[nvenc] Error: NVENC library file is not found. Please ensure NV driver is installed. \n");
		return false;
	}

	typedef NVENCSTATUS(NVENCAPI *NvEncodeAPIGetMaxSupportedVersion_Type)(uint32_t*);
#if defined(_WIN32)
	NvEncodeAPIGetMaxSupportedVersion_Type NvEncodeAPIGetMaxSupportedVersion = (NvEncodeAPIGetMaxSupportedVersion_Type)GetProcAddress(hModule, "NvEncodeAPIGetMaxSupportedVersion");
#else
	NvEncodeAPIGetMaxSupportedVersion_Type NvEncodeAPIGetMaxSupportedVersion = (NvEncodeAPIGetMaxSupportedVersion_Type)dlsym(hModule, "NvEncodeAPIGetMaxSupportedVersion");
#endif

	uint32_t version = 0;
	uint32_t currentVersion = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
	NVENC_API_CALL(NvEncodeAPIGetMaxSupportedVersion(&version));
	if (currentVersion > version)
	{
		printf("[nvenc] Error: Current Driver Version does not support this NvEncodeAPI version, please upgrade driver");
		return false;
	}

	return true;
}

static void* nvenc_create()
{
	if (!is_supported())
	{
		return nullptr;
	}

	struct nvenc_data *enc = new nvenc_data;

	HRESULT hr = S_OK;

	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)enc->factory.GetAddressOf());
	for (int gpuIndex = 0; gpuIndex <= 1; gpuIndex++)
	{
		hr = enc->factory->EnumAdapters(gpuIndex, enc->adapter.GetAddressOf());
		if (FAILED(hr))
		{
			goto failed;
		}
		else
		{
			char szDesc[128] = { 0 };
			DXGI_ADAPTER_DESC adapterDesc;
			enc->adapter->GetDesc(&adapterDesc);
			wcstombs(szDesc, adapterDesc.Description, sizeof(szDesc));
			//printf("%s\n", szDesc);
			if (strstr(szDesc, "NVIDIA") == NULL)
			{
				continue;
			}
		}

		hr = D3D11CreateDevice(enc->adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
							enc->device.GetAddressOf(), NULL, enc->context.GetAddressOf());
		if (SUCCEEDED(hr))
		{
			break;
		}
	}

	if (enc->adapter.Get() == nullptr)
	{
		printf("[nvenc] Error: Failed to create d3d11 device. \n");
		goto failed;
	}

	return enc;

failed:
	if (enc != nullptr) 
	{
		delete enc;
	}
	return nullptr;
}

static void nvenc_destroy(void **nvenc_data)
{
	struct nvenc_data *enc = (struct nvenc_data *)(*nvenc_data);

	enc->mutex.lock();

	if (enc->nvenc != nullptr)
	{
		//enc->nvenc->DestroyEncoder();
		delete enc->nvenc;
	}

	enc->mutex.unlock();

	delete *nvenc_data;
	*nvenc_data = nullptr;
}

static bool nvenc_init(void *nvenc_data, void *encoder_config)
{
	if (nvenc_data == nullptr)
	{
		return false;
	}

	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;
	struct encoder_config* config = (struct encoder_config*)encoder_config;

	std::lock_guard<std::mutex> locker(enc->mutex);
	if (enc->nvenc != nullptr)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
	desc.Width = config->width;
	desc.Height = config->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = config->format;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	HRESULT hr = enc->device.Get()->CreateTexture2D(&desc, NULL, enc->texture.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[nvenc] Error: Failed to create texture. \n");
		return false;
	}

	enc->width = config->width;
	enc->height = config->height;
	enc->framerate = config->framerate;
	enc->format = config->format;
	enc->codec = config->codec;
	enc->gop = config->gop;
	enc->bitrate = config->bitrate;

	NV_ENC_BUFFER_FORMAT eBufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
	if (enc->format == DXGI_FORMAT_NV12)
	{
		eBufferFormat = NV_ENC_BUFFER_FORMAT_NV12;
	}
	else if (enc->format == DXGI_FORMAT_B8G8R8A8_UNORM)
	{
		eBufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
	}
	else
	{
		printf("[nvenc] Error: Unsupported dxgi format. \n");
		return false;
	}

	GUID codecId = NV_ENC_CODEC_H264_GUID;
	if (enc->codec == "h264")
	{
		codecId = NV_ENC_CODEC_H264_GUID;
	}
	else if (enc->codec == "hevc")
	{
		codecId = NV_ENC_CODEC_HEVC_GUID;
	}
	else
	{
		printf("[nvenc] Error: Unsupported codec. \n");
		return false;
	}

	enc->nvenc = new NvEncoderD3D11(enc->device.Get(), enc->width, enc->height, eBufferFormat);

	NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
	NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
	initializeParams.encodeConfig = &encodeConfig;	
	enc->nvenc->CreateDefaultEncoderParams(&initializeParams, codecId, NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID);

	initializeParams.maxEncodeWidth = enc->width;
	initializeParams.maxEncodeHeight = enc->height;
	initializeParams.frameRateNum = enc->framerate;
	initializeParams.encodeConfig->gopLength = enc->gop;

	initializeParams.encodeConfig->rcParams.averageBitRate = enc->bitrate * 5 / 6;
	initializeParams.encodeConfig->rcParams.maxBitRate = enc->bitrate;
	//initializeParams.encodeConfig->rcParams.vbvBufferSize = enc->bitrate; // / (initializeParams.frameRateNum / initializeParams.frameRateDen);
	//initializeParams.encodeConfig->rcParams.vbvInitialDelay = initializeParams.encodeConfig->rcParams.vbvInitialDelay;
	initializeParams.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
	
	enc->nvenc->CreateEncoder(&initializeParams);

	return true;
}

int nvenc_encode_texture(void *nvenc_data, ID3D11Texture2D *texture, uint8_t* buf, uint32_t maxBufSize)
{
	if (nvenc_data == nullptr)
	{
		return -1;
	}

	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;	

	std::lock_guard<std::mutex> locker(enc->mutex);
	if (enc->nvenc == nullptr)
	{
		return -1;
	}

	std::vector<std::vector<uint8_t>> vPacket;
	const NvEncInputFrame* encoderInputFrame = enc->nvenc->GetNextInputFrame();
	ID3D11Texture2D *pTexBgra = reinterpret_cast<ID3D11Texture2D*>(encoderInputFrame->inputPtr);
	enc->context->CopyResource(pTexBgra, texture);
	enc->nvenc->EncodeFrame(vPacket);

	int frameSize = 0;
	for (std::vector<uint8_t> &packet : vPacket)
	{
		if (frameSize + packet.size() < maxBufSize)
		{
			memcpy(buf+frameSize, packet.data(), packet.size());
			frameSize += (int)packet.size();
		}
		else
		{
			break;
		}
	}

	return frameSize;
}

int nvenc_get_sequence_params(void *nvenc_data, uint8_t* buf, uint32_t maxBufSize)
{
	if (nvenc_data == nullptr)
	{
		return 0;
	}

	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;

	std::lock_guard<std::mutex> locker(enc->mutex);
	if (enc->nvenc != nullptr)
	{
		std::vector<uint8_t> seq_params;
		enc->nvenc->GetSequenceParams(seq_params);
		if (seq_params.size() > 0 && seq_params.size() < maxBufSize)
		{
			memcpy(buf, seq_params.data(), seq_params.size());
			return seq_params.size();
		}		
	}

	return 0;
}

static ID3D11Device* get_device(void *nvenc_data)
{	
	if (nvenc_data == nullptr)
	{
		return nullptr;
	}

	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;

	std::lock_guard<std::mutex> locker(enc->mutex);
	return enc->device.Get();
}

static ID3D11Texture2D* get_texture(void *nvenc_data)
{
	if (nvenc_data == nullptr)
	{
		return nullptr;
	}

	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;

	std::lock_guard<std::mutex> locker(enc->mutex);
	return enc->texture.Get();
}

struct encoder_info nvenc_info = {
	is_supported,
	nvenc_create,
	nvenc_destroy,
	nvenc_init,
	nvenc_encode_texture,
	nvenc_get_sequence_params,
	get_device,
	get_texture
};
