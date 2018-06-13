// 2018-6-12
// PHZ

// 支持本地RTSP转发和RTSP推流两种模式
// 支持RTMP推流

#include "DesktopSharing.h"
#include "xop/xop.h"

#define RTSP_PUSH_TEST "rtsp://192.168.77.86:554/test" // RTSP推流地址, 在EasyDarwin下测试通过
#define RTMP_PUSH_TEST "rtmp://192.168.77.101:1935/live/test" // RTMP推流地址, 在SRS下测试通过

using namespace xop;

int main()
{
    XOP_Init(); //WSAStartup

    if (DesktopSharing::Instance().Init("live", 554)) // 本地RTSP服务器转发地址rtsp://ip/live
    {
        DesktopSharing::Instance().startRtspPusher(RTSP_PUSH_TEST); //启动RTSP推流
        DesktopSharing::Instance().startRtmpPusher(RTMP_PUSH_TEST); //启动RTMP推流
        DesktopSharing::Instance().Start();
    }

    DesktopSharing::Instance().Exit();

    getchar();
    return 0;
}

