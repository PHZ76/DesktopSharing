// PHZ
// 2019-7-22

#include "DXGIScreenCapture.h"
#include <fstream> 

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")


DXGIScreenCapture::DXGIScreenCapture()
	: m_initialized(false)
	, m_isEnabeld(false)
	, m_threadPtr(nullptr)
	, m_textureHandle(nullptr)
	, m_bgraPtr(nullptr)
	, m_bgraSize(0)
{
	memset(&m_dxgiDesc, 0, sizeof(m_dxgiDesc));
}

DXGIScreenCapture::~DXGIScreenCapture()
{

}

int DXGIScreenCapture::init(int displayIndex)
{
	if (m_initialized)
	{
		return -1;
	}

	HRESULT hr = S_OK;

	D3D_FEATURE_LEVEL featureLevel;
	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
							m_d3d11device.GetAddressOf(), &featureLevel, m_d3d11DeviceContext.GetAddressOf());
	if (FAILED(hr)) 
	{
		printf("[DXGIScreenCapture] Failed to create d3d11 device.\n");
		return -1;
	}

	Microsoft::WRL::ComPtr<IDXGIFactory> m_dxgiFactory;
	hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), (void **)m_dxgiFactory.GetAddressOf());
	if (FAILED(hr)) 
	{
		printf("[DXGIScreenCapture] Failed to create dxgi factory.\n");
		return -1;
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
	Microsoft::WRL::ComPtr<IDXGIOutput> dxgiOutput;
	int index = displayIndex;
	do
	{
		if (m_dxgiFactory->EnumAdapters(index, dxgiAdapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND)
		{
			int i = 0;
			while (dxgiAdapter->EnumOutputs(i, dxgiOutput.GetAddressOf()) != DXGI_ERROR_NOT_FOUND)
			{
				i++;
				if (dxgiOutput.Get() != nullptr)
				{
					break;
				}
			}
		}
	} while (0);


	if (dxgiAdapter.Get() == nullptr)
	{
		printf("[DXGIScreenCapture] DXGI adapter not found.\n");
		return -1;
	}

	if (dxgiOutput.Get() == nullptr)
	{
		printf("[DXGIScreenCapture] DXGI output not found.\n");
		return -1;
	}

	Microsoft::WRL::ComPtr<IDXGIOutput1> dxgiOutput1;
	hr = dxgiOutput.Get()->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void **>(dxgiOutput1.GetAddressOf()));
	if (FAILED(hr)) 
	{
		printf("[DXGIScreenCapture] Failed to query interface dxgiOutput1.\n");
		return -1;
	}

	hr = dxgiOutput1->DuplicateOutput(m_d3d11device.Get(), m_dxgi0utputDuplication.GetAddressOf());
	if (FAILED(hr)) 
	{
		printf("[DXGIScreenCapture] Failed to get duplicate output.\n");
		return -1;
	}

	 m_dxgi0utputDuplication->GetDesc(&m_dxgiDesc);

	if (createSharedTexture())
	{
		return -1;
	}
	
	m_initialized = true;
	return 0;
}

int DXGIScreenCapture::createSharedTexture()
{
	D3D11_TEXTURE2D_DESC desc = {0};
	desc.Width = m_dxgiDesc.ModeDesc.Width;
	desc.Height = m_dxgiDesc.ModeDesc.Height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.BindFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED; 

	HRESULT hr = m_d3d11device->CreateTexture2D(&desc, nullptr, m_sharedTexture.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[DXGIScreenCapture] Failed to create texture.\n");
		return -1;
	}

	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;
	hr = m_d3d11device->CreateTexture2D(&desc, nullptr, m_rgbaTexture.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[DXGIScreenCapture] Failed to create texture.\n");
		return -1;
	}

	Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
	hr = m_sharedTexture->QueryInterface(__uuidof(IDXGIResource), reinterpret_cast<void **>(dxgiResource.GetAddressOf()));
	if (FAILED(hr))
	{
		printf("[DXGIScreenCapture] Failed to query IDXGIResource interface from texture.\n");
		return -1;
	}

	hr = dxgiResource->GetSharedHandle(&m_textureHandle);
	if (FAILED(hr))
	{
		printf("[DXGIScreenCapture] Failed to get shared handle.\n");
		return -1;
	}

	return 0;
}

int DXGIScreenCapture::exit()
{
	return 0;
}

int DXGIScreenCapture::start()
{
	if (!m_initialized)
	{
		return -1;
	}

	if (m_isEnabeld)
	{
		return 0;
	}

	m_isEnabeld = true;
	aquireFrame();
	m_threadPtr.reset(new std::thread([this] {
		while (m_isEnabeld)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
			aquireFrame();
		}
	}));

	return 0;
}

int DXGIScreenCapture::stop()
{
	m_isEnabeld = false;

	if (m_threadPtr != nullptr)
	{
		m_threadPtr->join();
		m_threadPtr = nullptr;
	}

	return 0;
}

int DXGIScreenCapture::aquireFrame()
{
	Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;
	memset(&frameInfo, 0, sizeof(frameInfo));
		
	m_dxgi0utputDuplication->ReleaseFrame();
	HRESULT hr = m_dxgi0utputDuplication->AcquireNextFrame(0, &frameInfo, dxgiResource.GetAddressOf());
	
	if (FAILED(hr))
	{
		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			return -1;
		}
		if (hr == DXGI_ERROR_INVALID_CALL)
		{
			return -1;
		}
		if (hr == DXGI_ERROR_ACCESS_LOST)
		{
			return -1;
		}
		
		return -1;
	}

	if (frameInfo.AccumulatedFrames == 0 || frameInfo.LastPresentTime.QuadPart == 0)
	{
		// No image update, only cursor moved.
	}

	if (!dxgiResource.Get())
	{
		return -1;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> outputTexture;
	hr = dxgiResource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(outputTexture.GetAddressOf()));
	if (FAILED(hr))
	{
		return -1;
	}

	std::lock_guard<std::mutex> locker(m_mutex);

	m_bgraSize = m_dxgiDesc.ModeDesc.Width * m_dxgiDesc.ModeDesc.Height * 4;

	D3D11_MAPPED_SUBRESOURCE dsec = { 0 };
	m_d3d11DeviceContext->CopyResource(m_rgbaTexture.Get(), outputTexture.Get());
	hr = m_d3d11DeviceContext->Map(m_rgbaTexture.Get(), 0, D3D11_MAP_READ, 0, &dsec);
	if (SUCCEEDED(hr))
	{
		if (dsec.pData != NULL)
		{
			m_bgraPtr.reset(new uint8_t[m_bgraSize]);
			memcpy(m_bgraPtr.get(), dsec.pData, m_bgraSize);
		}
		m_d3d11DeviceContext->Unmap(m_rgbaTexture.Get(), 0);
	}

	m_d3d11DeviceContext->CopyResource(m_sharedTexture.Get(), outputTexture.Get());

	return 0;
}

int DXGIScreenCapture::captureFrame(std::shared_ptr<uint8_t>& bgraPtr, uint32_t& size)
{
	std::lock_guard<std::mutex> locker(m_mutex);

	if (!m_isEnabeld)
	{
		return -1;
	}

	if (m_bgraPtr == nullptr || m_bgraSize == 0)
	{
		return -1;
	}

	bgraPtr = m_bgraPtr;
	size = m_bgraSize;
	return 0;
}

int DXGIScreenCapture::captureFrame(ID3D11Device* device, ID3D11Texture2D* texture)
{
	std::lock_guard<std::mutex> locker(m_mutex);

	if (device==nullptr || texture==nullptr)
	{
		return -1;
	}

	if (!m_isEnabeld)
	{
		return -1;
	}

	if (m_bgraSize == 0)
	{
		return -1;
	}

	Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext;
	device->GetImmediateContext(deviceContext.GetAddressOf());
	if (deviceContext.Get() == nullptr)
	{
		printf("[DXGIScreenCapture] Failed to get context from device.\n");
		return -1;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> inputTexture;
	HRESULT hr = device->OpenSharedResource((HANDLE)(uintptr_t)m_textureHandle, __uuidof(ID3D11Texture2D),
											reinterpret_cast<void **>(inputTexture.GetAddressOf()));
	if (FAILED(hr))
	{
		printf("[DXGIScreenCapture] Failed to open shared resource from device.\n");
		return -1;
	}

	deviceContext->CopyResource(texture, inputTexture.Get());
	return 0;
}

int DXGIScreenCapture::captureImage(std::string pathname)
{
	std::lock_guard<std::mutex> locker(m_mutex);

	if (!m_isEnabeld)
	{
		return -1;
	}

	if (m_bgraPtr == nullptr || m_bgraSize == 0)
	{
		return -1;
	}
	
	std::ofstream fpOut(pathname.c_str(), std::ios::out | std::ios::binary);
	if (!fpOut)
	{
		printf("[DXGIScreenCapture] capture image failed, open %s failed.\n", pathname.c_str());
		return -1;
	}

	unsigned char fileHeader[54] = {  
		0x42, 0x4d, 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0,  /*file header*/
		40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 32, 0, /*info header*/
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0
	};

	uint32_t imageWidth  = m_dxgiDesc.ModeDesc.Width;
	uint32_t imageHeight = m_dxgiDesc.ModeDesc.Height;
	uint32_t imageSize  = m_bgraSize;
	uint32_t fileSize = sizeof(fileHeader) + imageSize;

	fileHeader[2] = (uint8_t)fileSize;
	fileHeader[3] = fileSize >> 8;
	fileHeader[4] = fileSize >> 16;
	fileHeader[5] = fileSize >> 24;

	fileHeader[18] = (uint8_t)imageWidth;
	fileHeader[19] = imageWidth >> 8;
	fileHeader[20] = imageWidth >> 16;
	fileHeader[21] = imageWidth >> 24;

	fileHeader[22] = (uint8_t)imageHeight;
	fileHeader[23] = imageHeight >> 8;
	fileHeader[24] = imageHeight >> 16;
	fileHeader[25] = imageHeight >> 24;

	fpOut.write((char*)fileHeader, 54);

	char *imageData = (char *)m_bgraPtr.get();
	for (int h = imageHeight-1; h >= 0; h--)
	{
		fpOut.write(imageData+h*imageWidth*4, imageWidth *4);
	}

	fpOut.close();
	return 0;
}