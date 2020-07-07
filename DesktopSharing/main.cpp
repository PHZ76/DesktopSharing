#include "ScreenLive.h"
#include "MainWindow.h"

#define ENABLE_SDL_WINDOW 1

#if ENABLE_SDL_WINDOW

#ifndef _DEBUG
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#endif

static uint32_t OnPaint(uint32_t interval, void *param)
{
	MainWindow* window = reinterpret_cast<MainWindow*>(param);
	if (window) {
		std::vector<uint8_t> bgra_image;
		uint32_t width = 0;
		uint32_t height = 0;
		if (ScreenLive::Instance().GetScreenImage(bgra_image, width, height)) {
			window->UpdateARGB(&bgra_image[0], width, height);
		}
	}

	return 100;
}

int main(int argc, char **argv)
{
	MainWindow window;
	SDL_TimerID timer_id = 0;

	if (window.Create()) {
		if (ScreenLive::Instance().StartCapture() >= 0) {
			timer_id = SDL_AddTimer(1000, OnPaint, &window);
		}		
		else {
			return -1;
		}
	}

	bool done = false;
	while (!done && window.IsWindow()) {
		SDL_Event event;
		if (SDL_WaitEvent(&event)) {
			window.Porcess(event);

			switch (event.type)
			{
				case SDL_QUIT: {
					done = true;
					break;
				}

				default: {
					break;
				}
			}
		}
	}
	
	if (timer_id) {
		SDL_RemoveTimer(timer_id);
	}

	window.Destroy();
	ScreenLive::Instance().Destroy();
	return 0;
}

#else 

#define RTSP_PUSHER_TEST "rtsp://127.0.0.1:554/test" 
#define RTMP_PUSHER_TEST "rtmp://127.0.0.1:1935/live/02"  

int main(int argc, char **argv)
{
	AVConfig avconfig;
	avconfig.bitrate_bps = 4000000; // video bitrate
	avconfig.framerate = 25;        // video framerate
	avconfig.codec = "h264";  // hardware encoder: "h264_nvenc";        

	LiveConfig live_config;

	// server
	live_config.ip = "0.0.0.0";
	live_config.port = 8554;
	live_config.suffix = "live";

	// pusher
	live_config.rtmp_url = RTMP_PUSHER_TEST;
	live_config.rtsp_url = RTSP_PUSHER_TEST;

	if (!ScreenLive::Instance().Init(avconfig)) {
		getchar();
		return 0;
	}

	ScreenLive::Instance().StartLive(SCREEN_LIVE_RTSP_SERVER, live_config);
	//ScreenLive::Instance().StartLive(SCREEN_LIVE_RTSP_PUSHER, live_config);
	//ScreenLive::Instance().StartLive(SCREEN_LIVE_RTMP_PUSHER, live_config);

	while (1) {		
		//if (ScreenLive::Instance().IsConnected(SCREEN_LIVE_RTMP_PUSHER)) {

		//}

		//if (ScreenLive::Instance().IsConnected(SCREEN_LIVE_RTSP_PUSHER)) {

		//}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	ScreenLive::Instance().StopLive(SCREEN_LIVE_RTSP_SERVER);
	//ScreenLive::Instance().StopLive(SCREEN_LIVE_RTSP_PUSHER);
	//ScreenLive::Instance().StopLive(SCREEN_LIVE_RTMP_PUSHER);

	ScreenLive::Instance().Destroy();

	getchar();
	return 0;
}

#endif