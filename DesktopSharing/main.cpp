#include "ScreenLive.h"

#define RTSP_PUSHER_TEST "rtsp://127.0.0.1:554/test" 
#define RTMP_PUSHER_TEST "rtmp://127.0.0.1:1935/live/02"  

int main(int argc, char **argv)
{
	AVConfig avconfig;
	avconfig.bitrate_bps = 4000000; // video bitrate
	avconfig.framerate = 25;        // video framerate
	avconfig.codec = "h264_nvenc";  // hardware encoder: "h264_nvenc";        

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

	ScreenLive::Instance().Init(avconfig);
	ScreenLive::Instance().Start(SCREEN_LIVE_RTSP_SERVER, live_config);
	//ScreenLive::Instance().Start(SCREEN_LIVE_RTSP_PUSHER, live_config);
	//ScreenLive::Instance().Start(SCREEN_LIVE_RTMP_PUSHER, live_config);

	while (1) {		
		//if (ScreenLive::Instance().IsConnected(SCREEN_LIVE_RTMP_PUSHER)) {

		//}

		//if (ScreenLive::Instance().IsConnected(SCREEN_LIVE_RTSP_PUSHER)) {

		//}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	ScreenLive::Instance().Stop(SCREEN_LIVE_RTSP_SERVER);
	//ScreenLive::Instance().Stop(SCREEN_LIVE_RTSP_PUSHER);
	//ScreenLive::Instance().Stop(SCREEN_LIVE_RTMP_PUSHER);

	ScreenLive::Instance().Destroy();

	getchar();
	return 0;
}

