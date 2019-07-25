// PHZ
// 2019-7-22

#ifndef DXGI_SCREEN_CAPTURE_H
#define DXGI_SCREEN_CAPTURE_H

#include <cstdio>
#include <cstdint>
#include <string>
#include <mutex>
#include <thread>
#include <memory>
#include <wrl.h>
#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_2.h>

class DXGIScreenCapture
{
public:
	DXGIScreenCapture();
	virtual ~DXGIScreenCapture();
	DXGIScreenCapture &operator=(const DXGIScreenCapture &) = delete;
	DXGIScreenCapture(const DXGIScreenCapture &) = delete;

	int init(int displayIndex = 0);
	int exit();
	int start();
	int stop();

	inline int getWidth()  { return m_dxgiDesc.ModeDesc.Width; }
	inline int getHeight() { return m_dxgiDesc.ModeDesc.Height; }

	int captureFrame(std::shared_ptr<uint8_t>& bgraPtr, uint32_t& size);
	int captureFrame(ID3D11Device* device, ID3D11Texture2D* texture);

	int captureImage(std::string pathname);

	inline ID3D11Device* getID3D11Device() { return m_d3d11device.Get(); }
	inline ID3D11DeviceContext* getID3D11DeviceContext() { return m_d3d11DeviceContext.Get(); }

private:
	int createSharedTexture();
	int aquireFrame();

	bool m_initialized;
	bool m_isEnabeld;
	std::mutex m_mutex;
	std::shared_ptr<std::thread> m_threadPtr;
	std::shared_ptr<uint8_t> m_bgraPtr;
	uint32_t m_bgraSize;
	DXGI_OUTDUPL_DESC m_dxgiDesc;
	HANDLE m_textureHandle;
	Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11device;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_sharedTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_rgbaTexture;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
	Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_dxgi0utputDuplication;
};

#endif