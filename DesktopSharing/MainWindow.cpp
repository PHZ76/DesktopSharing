#include "MainWindow.h"
#include <mutex>

MainWindow::MainWindow()
{
	avconfig_.bitrate_bps = 4000000; // video bitrate
	avconfig_.framerate = 25;        // video framerate
	avconfig_.codec = "";  // hardware encoder: "h264_nvenc";    
}

MainWindow::~MainWindow()
{

}

bool MainWindow::Create()
{
	static std::once_flag init_flag;
	std::call_once(init_flag, [=]() {
		SDL_assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
	});

	int window_flag = SDL_WINDOW_ALLOW_HIGHDPI;
	window_ = SDL_CreateWindow("Screen Live", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		kWindowWidth, kWindowHeight, window_flag);

	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (SDL_GetWindowWMInfo(window_, &wm_info)) {
		window_handle_ = wm_info.info.win.window;
	}

	if (!InitD3D()) {
		Destroy();
		return false;
	}

	/* disable chinese input */
	if (window_handle_) {
		ImmAssociateContext((HWND)window_handle_, nullptr);
	}

	return (window_handle_ && window_);
}

void MainWindow::Destroy()
{
	if (window_) {
		ClearD3D();
		SDL_DestroyWindow(window_);
		window_ = nullptr;
		window_handle_ = nullptr;
		//SDL_Quit();
	}
}

bool MainWindow::IsWindow() const
{
	return window_ != nullptr;
}

bool MainWindow::InitD3D()
{
	int driver_index = -1;
	int driver_count = SDL_GetNumRenderDrivers();
	int renderer_flags = SDL_RENDERER_SOFTWARE;

	for (int i = 0; i < driver_count; i++) {
		SDL_RendererInfo info;
		if (SDL_GetRenderDriverInfo(i, &info) < 0) {
			continue;
		}

		if (strcmp(info.name, "direct3d") == 0) {
			driver_index = i;
			if (info.flags & SDL_RENDERER_ACCELERATED) {
				renderer_flags = SDL_RENDERER_ACCELERATED;
			}
		}
	}

	if (driver_index < 0) {
		return false;
	}

	renderer_ = SDL_CreateRenderer(window_, driver_index, renderer_flags);
	SDL_assert(renderer_ != nullptr);

	device_ = SDL_RenderGetD3D9Device(renderer_);
	SDL_assert(device_ != nullptr);

	SDL_SetRenderDrawColor(renderer_, 114, 144, 154, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer_);
	SDL_RenderPresent(renderer_);

	overlay_ = new Overlay;
	if (!overlay_->Init(window_, device_)) {
		delete overlay_;
		overlay_ = nullptr;
	}
	overlay_->SetRect(0, 0 + kVideoHeight, kOverlayWidth, kOverlayHeight);
	overlay_->RegisterObserver(this);
	return true;
}

void MainWindow::ClearD3D()
{
	if (texture_ != nullptr) {
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
		texture_format_ = SDL_PIXELFORMAT_UNKNOWN;
		texture_width_ = 0;
		texture_height_ = 0;
	}

	if (renderer_ != nullptr) {
		SDL_DestroyRenderer(renderer_);
		renderer_ = nullptr;
	}

	if (overlay_) {
		delete overlay_;
		overlay_ = nullptr;
	}
}

void MainWindow::Porcess(SDL_Event& event)
{
	if (IsWindow()) {
		if (overlay_) {
			Overlay::Process(&event);
		}		
	}	
}

bool MainWindow::UpdateARGB(const uint8_t* data, uint32_t width, uint32_t height)
{
	if (!IsWindow()) {
		return false;
	}

	if (texture_format_ != SDL_PIXELFORMAT_ARGB8888 ||
		(texture_width_ != width) || (texture_height_ != height)) {
		if (texture_) {
			SDL_DestroyTexture(texture_);
			texture_ = nullptr;
		}	
	}

	if (!texture_) {		
		texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING, width, height);
		SDL_assert(texture_ != nullptr);
		texture_format_ = SDL_PIXELFORMAT_ARGB8888;
		texture_width_ = width;
		texture_height_ = height;
	}

	if (texture_) {
		char* pixels = nullptr;
		int pitch = 0;

		int ret = SDL_LockTexture(texture_, nullptr, (void**)&pixels, &pitch);
		SDL_assert(ret >= 0);
	
		memcpy(pixels, data, texture_width_ * texture_height_ * 4);
		SDL_UnlockTexture(texture_);

		//SDL_SetRenderDrawColor(renderer_, 114, 144, 154, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer_);

		SDL_Rect rect = { 0, 0, kVideoWidth, kVideoHeight };
		SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
		if (overlay_) {
			overlay_->Render();
		}
		SDL_RenderPresent(renderer_);

		return true;
	}

	return false;
}

bool MainWindow::StartLive(int& event_type, std::vector<std::string>& settings)
{
	AVConfig avconfig;
	avconfig.bitrate_bps = avconfig_.bitrate_bps; 
	avconfig.framerate = avconfig_.framerate;
	avconfig.codec = "h264";  // hardware encoder: "h264_nvenc";    
	if (settings[0] == "nvenc") {
		if (nvenc_info.is_supported()) {
			avconfig.codec = "h264_nvenc";
		}
	}

	/* reset video encoder */
	if (avconfig_.codec != avconfig.codec) {
		ScreenLive::Instance().StopEncoder();
		ScreenLive::Instance().StartEncoder(avconfig);
		ScreenLive::Instance().StopLive(SCREEN_LIVE_RTSP_SERVER);
		ScreenLive::Instance().StopLive(SCREEN_LIVE_RTSP_PUSHER);
		ScreenLive::Instance().StopLive(SCREEN_LIVE_RTMP_PUSHER);
		overlay_->SetLiveState(RTSP_SERVER_EVENT, false);
		overlay_->SetLiveState(RTSP_PUSHER_EVENT, false);
		overlay_->SetLiveState(RTMP_PUSHER_EVENT, false);
		avconfig_.codec = avconfig.codec;		
	}

	if (!ScreenLive::Instance().IsEncoderInitialized()) {
		return false;
	}

	LiveConfig live_config;
	bool ret = false;

	if (event_type == RTSP_SERVER_EVENT) {
		live_config.ip = settings[1];
		live_config.port = atoi(settings[2].c_str());
		live_config.suffix = settings[3];
		ret = ScreenLive::Instance().StartLive(SCREEN_LIVE_RTSP_SERVER, live_config);
	}
	else if (event_type == RTSP_PUSHER_EVENT) {
		live_config.rtsp_url = settings[1];		
		ret = ScreenLive::Instance().StartLive(SCREEN_LIVE_RTSP_PUSHER, live_config);
	}
	else if (event_type == RTMP_PUSHER_EVENT) {
		live_config.rtmp_url = settings[1];
		ret = ScreenLive::Instance().StartLive(SCREEN_LIVE_RTMP_PUSHER, live_config);
	}

	return ret;
}

void MainWindow::StopLive(int event_type)
{
	if (event_type == RTSP_SERVER_EVENT) {
		ScreenLive::Instance().StopLive(SCREEN_LIVE_RTSP_SERVER);
	}
	else if (event_type == RTSP_PUSHER_EVENT) {
		ScreenLive::Instance().StopLive(SCREEN_LIVE_RTSP_PUSHER);
	}
	else if (event_type == RTMP_PUSHER_EVENT) {
		ScreenLive::Instance().StopLive(SCREEN_LIVE_RTMP_PUSHER);
	}
}