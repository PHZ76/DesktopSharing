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

	uint32_t width;
	uint32_t height;
	uint32_t framerate;
	uint32_t gop;
	std::string codec;
	DXGI_FORMAT format;
	NvEncoderD3D11 *nvenc;
};

static bool is_supported(void)
{
#if defined(_WIN32)
#if defined(_WIN64)
	HMODULE hModule = LoadLibrary(TEXT("nvEncodeAPI64.dll"));
#else
	HMODULE hModule = LoadLibrary(TEXT("nvEncodeAPI.dll"));
#endif
#else
	return false;
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

static void nvenc_destroy(void *nvenc_data)
{
	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;

	enc->nvenc->DestroyEncoder();
	delete enc->nvenc;

	enc->device->Release();
	enc->context->Release();
	enc->adapter->Release();
	enc->factory->Release();
}

static bool nvenc_init(void *nvenc_data, void *encoder_config)
{
	if (nvenc_data == nullptr)
	{
		return false;
	}

	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;
	struct encoder_config* config = (struct encoder_config*)encoder_config;

	enc->width = config->width;
	enc->height = config->height;
	enc->framerate = config->framerate;
	enc->format = config->format;
	enc->codec = config->codec;

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
	enc->nvenc->CreateEncoder(&initializeParams);
}

int nvenc_encode_texture(void *nvenc_data, ID3D11Texture2D *texture, uint8_t* buf, uint32_t maxBufSize)
{
	if (nvenc_data == nullptr)
	{
		return -1;
	}

	struct nvenc_data *enc = (struct nvenc_data *)nvenc_data;
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

struct encoder_info nvenc_info = {
	is_supported,
	nvenc_create,
	nvenc_destroy,
	nvenc_init,
	nvenc_encode_texture
};
