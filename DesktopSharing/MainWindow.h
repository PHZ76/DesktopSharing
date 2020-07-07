#ifndef SCREEN_LIVE_MAIN_WINDOW_H
#define SCREEN_LIVE_MAIN_WINDOW_H

#include "SDL.h"
#include "SDL_syswm.h"
#include "Overlay.h"
#include "ScreenLive.h"

class MainWindow : public OverlayCallack
{
public:
	MainWindow();
	virtual ~MainWindow();

	bool Create();
	void Destroy();
	bool IsWindow() const;

	void Porcess(SDL_Event& event);

	bool UpdateARGB(const uint8_t* data, uint32_t width, uint32_t height);

private:
	bool InitD3D();
	void ClearD3D();

	virtual bool StartLive(int& event_type, std::vector<std::string>& settings);
	virtual void StopLive(int event_type);

	SDL_Window* window_   = nullptr;
	HANDLE window_handle_ = nullptr;

	Overlay* overlay_ = nullptr;
	AVConfig avconfig_;

	SDL_Renderer* renderer_   = nullptr;
	SDL_Texture*  texture_    = nullptr;
	IDirect3DDevice9* device_ = nullptr;
	int texture_format_ = SDL_PIXELFORMAT_UNKNOWN;
	uint32_t texture_width_  = 0;
	uint32_t texture_height_ = 0;

	static const int kWindowWidth   = 960;
	static const int kWindowHeight  = 740;
	static const int kVideoWidth    = kWindowWidth;
	static const int kVideoHeight   = 540;
	static const int kOverlayWidth  = kWindowWidth;
	static const int kOverlayHeight = kWindowHeight - kVideoHeight;
};

#endif