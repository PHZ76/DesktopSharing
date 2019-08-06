// 2019-8-1
// PHZ

// 支持本地RTSP转发和RTSP推流两种模式
// 支持RTMP推流
// 本地RTSP地址 rtsp://ip/live

#include "DesktopSharing.h"

#define RTSP_PUSH_TEST "rtsp://192.168.43.213:554/test" // RTSP推流地址, 在EasyDarwin下测试通过
#define RTMP_PUSH_TEST "rtmp://192.168.43.191/live/01"  // RTMP推流地址, 在SRS下测试通过

using namespace xop;

int main(int argc, char **argv)
{
	if (!DesktopSharing::instance().init())
	{
		getchar();
		return 0;
	}

	DesktopSharing::instance().start();
	DesktopSharing::instance().startRtspServer("live", 8554);
	//DesktopSharing::instance().startRtspPusher(RTSP_PUSH_TEST); //启动RTSP推流
	//DesktopSharing::instance().startRtmpPusher(RTMP_PUSH_TEST); //启动RTMP推流

	while (1)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	DesktopSharing::instance().stop();
    DesktopSharing::instance().exit();
    getchar();
    return 0;
}

