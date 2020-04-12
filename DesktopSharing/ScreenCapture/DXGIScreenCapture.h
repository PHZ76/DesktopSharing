// PHZ
// 2020-4-7

#ifndef DXGI_SCREEN_CAPTURE_H
#define DXGI_SCREEN_CAPTURE_H

#include <cstdio>
#include <cstdint>
#include <string>
#include <mutex>
#include <thread>
#include <memory>
#include <vector>
#include <wrl.h>
#include <dxgi.h>
#include <d3d11.h>
#include <dxgi1_2.h>

class DXGIScreenCapture
{
public:
	DXGIScreenCapture();
	virtual ~DXGIScreenCapture();

	bool Init(int displayIndex = 0);
	bool Destroy();

	uint32_t GetWidth()  const { return dxgi_desc_.ModeDesc.Width; }
	uint32_t GetHeight() const { return dxgi_desc_.ModeDesc.Height; }

	bool CaptureFrame(std::vector<uint8_t>& bgra_image);
	bool GetTextureHandle(HANDLE* handle, int* lockKey, int* unlockKey);
	bool CaptureImage(std::string pathname);
	
	inline ID3D11Device* GetD3D11Device() { return d3d11_device_.Get(); }
	inline ID3D11DeviceContext* GetD3D11DeviceContext() { return d3d11_context_.Get(); }

	bool CaptureStarted() const
	{ return is_started_; }

private:
	int StartCapture();
	int StopCapture();
	int CreateSharedTexture();
	int AquireFrame();

	bool is_initialized_;
	bool is_started_;
	std::unique_ptr<std::thread> thread_ptr_;

	std::mutex mutex_;
	std::shared_ptr<uint8_t> image_ptr_; // bgra
	uint32_t image_size_;

	// d3d resource
	DXGI_OUTDUPL_DESC dxgi_desc_;
	HANDLE texture_handle_;
	int key_;
	Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context_;
	Microsoft::WRL::ComPtr<IDXGIOutputDuplication> dxgi_output_duplication_;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> shared_texture_;
	Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> rgba_texture_;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> gdi_texture_;
};

#endif