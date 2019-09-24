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
* 完成硬件编码(nvenc), 仅支持部分nvidia显卡。

后续计划
-
* 

编译环境
-
* win10, vs2017, windows-sdk-version-10.0.17134.0
* 项目使用的模块都是开源项目, 在vs2017下编译通过。

模块说明
-
* 屏幕采集: 使用DXGI抓屏技术，所以项目只适合运行在win8以上的系统。
* 音频采集: 使用WASAPI捕获声卡音频数据。
* 编码器: 使用开源项目 [ffmpeg4.0](https://ffmpeg.org/)，ffmpeg的dll文件太大，请到官网下载(4.0版本)。
* 硬件编码器: 使用 [Video-Codec-SDK](https://developer.nvidia.com/nvidia-video-codec-sdk), Version: 8.2。
* RTMP推流器: [rtmp](https://github.com/PHZ76/rtmp)。
* RTSP服务器,推流器: [RtspServer](https://github.com/PHZ76/RtspServer)。

VLC播放效果
-
![image](https://github.com/PHZ76/DesktopSharing/blob/master/pic/2.pic.jpg) 
