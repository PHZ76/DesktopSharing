# DesktopSharing

项目介绍
-
* 抓取屏幕和声卡的音视频数据，编码后进行RTSP转发, RTSP推流, RTMP推流。

目前情况
-
* 完成屏幕采集(DXGI)和H.264编码。
* 完成音频采集(WASAPI)和AAC编码。
* 完成RTSP本地转发音视频数据。
* 完成RTSP推流器。
* 完成RTMP推流器。
* 完成独显硬件编码(nvenc), 仅支持部分nvidia显卡。
* 完成核显硬件编码(qsv)。
* 完成简单的UI界面。

后续计划
-

编译环境
-
* win10, vs2017, windows-sdk-version-10.0.17134.0
* 项目使用的模块都是开源项目, 在vs2017/vs2019下编译通过。

模块说明
-
* 屏幕采集: DXGI(win8以上), GDI
* 音频采集: WASAPI
* 编码器: [ffmpeg4.0](https://ffmpeg.org/), Version: 4.0
* 独显硬件编码器: [Video-Codec-SDK](https://developer.nvidia.com/nvidia-video-codec-sdk), Version: 8.2
* 核显硬件编码器: [Media-SDK](https://github.com/Intel-Media-SDK/MediaSDK)
* RTMP推流器: [rtmp](https://github.com/PHZ76/rtmp)
* RTSP服务器,推流器: [RtspServer](https://github.com/PHZ76/RtspServer)
* UI界面: [SDL](https://github.com/SDL-mirror/SDL), [imgui](https://github.com/ocornut/imgui)

使用方式
-
* 将编译生成的exe文件放入run-env中，即可运行。

-
![image](https://github.com/PHZ76/DesktopSharing/blob/master/pic/2.pic.jpg) 
