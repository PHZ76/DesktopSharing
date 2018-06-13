#ifndef _VIDEO_CAPTURE_H
#define _VIDEO_CAPTURE_H

#include <cstdio>
#include <cstdint>
#include <memory>
#include <iostream>
#include <thread>
#include "xop/RingBuffer.h"

#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "screencapture/ScreenCapture.h" 

struct RGBAFrame
{
	RGBAFrame(uint32_t size = 100)
		: data(new uint8_t[size + 1024])
	{
		this->size = size;
	}

	uint32_t size = 0;
	uint32_t width;
	uint32_t height;
	std::shared_ptr<uint8_t> data;
};

class VideoCapture
{
public:
	VideoCapture & operator=(const VideoCapture &) = delete;
	VideoCapture(const VideoCapture &) = delete;
	static VideoCapture& Instance();
	~VideoCapture();

	bool Init(uint32_t framerate=25);
	void Exit();

	bool Start(); // 16ms
	void Stop();

	bool getFrame(RGBAFrame& frame);

	bool isCapturing()
	{
		if (_isInitialized && _capture && _capture->isStarted() == 0)
			return true;
		return false;
	}
	
	uint32_t Width() const
	{
		return _width;
	}

	uint32_t Height() const
	{
		return _height;
	}

private:
	VideoCapture();
    bool GetDisplaySetting(std::string& name);
    void Capture();
    static void FrameCallback(sc::PixelBuffer& buf);
    
	std::thread _thread;
	std::shared_ptr<sc::ScreenCapture> _capture;
	bool _isInitialized = false;
	uint32_t _framerate = 25;
	uint32_t _width = 0;
	uint32_t _height = 0;

	RGBAFrame _lastFrame;
	std::shared_ptr<RingBuffer<RGBAFrame>> _frameBuffer;
};

#endif
