# DesktopSharing

项目介绍
-
* 抓取屏幕和麦克风的音视频数据，编码后进行RTSP转发, RTSP推流, RTMP推流。

目前情况
-
* 完成屏幕采集和H.264编码。
* 完成音频采集和AAC编码。
* 完成RTSP本地转发音视频数据。
* 完成RTSP推流到流媒体服务器EasyDarwin。
* 完成RTMP推流到流媒体服务器SRS。

后续计划
-
* 增加硬件编码
* 增加声卡音频捕获
* RTMP转发

编译环境
-
* win10, vs2017, windows-sdk-version-10.0.17134.0
* 项目使用的模块都是开源项目, 在vs2017下编译通过。

设计思路
-
![image](https://github.com/PHZ76/DesktopSharing/blob/master/pic/1.pic.jpg) 

库文件说明
-
* 屏幕采集: 使用开源项目 [screen_capture](https://github.com/diederickh/screen_capture)，因为抓屏使用了DXGI技术， 所以项目只适合运行在win8以上的系统。
* 音频采集: 使用开源项目 [portaudio](http://www.portaudio.com/)。
* 编码器, RTMP推流器: 使用开源项目 [ffmpeg4.0](https://ffmpeg.org/)，ffmpeg的dll文件太大，请到官网下载(4.0版本)。
* RTSP服务器,推流器: [RtspServer](https://github.com/PHZ76/RtspServer)。

VLC播放效果
-
![image](https://github.com/PHZ76/DesktopSharing/blob/master/pic/2.pic.jpg) 
