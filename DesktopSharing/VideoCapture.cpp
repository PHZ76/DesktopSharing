#include "VideoCapture.h"
#include "net/log.h"

// screencapture
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "screencapture.lib")

using namespace std;

VideoCapture::VideoCapture()
	: _frameBuffer(new xop::RingBuffer<RGBAFrame>(10))
{
	_lastFrame.size = 0;
}

VideoCapture::~VideoCapture()
{

}

VideoCapture& VideoCapture::instance()
{
	static VideoCapture s_vc;
	return s_vc;
}

bool VideoCapture::init(uint32_t framerate)
{
	if (_isInitialized)
		return true;

	std::vector<sc::Display*> displays;
	sc::Settings settings;

	_capture.reset(new sc::ScreenCapture(VideoCapture::FrameCallback));

	if (0 != _capture->init()) 
	{
		return false;
	}

	if (0 != _capture->getDisplays(displays))
	{
		return false;
	}

	getDisplaySetting(displays[0]->name);
    
    settings.pixel_format = SC_BGRA;
	settings.display = 0;
	settings.output_width = _width;
	settings.output_height = _height;

	if (0 != _capture->configure(settings)) 
	{
		return false;
	}

	_isInitialized = true;
	_thread = std::thread(&VideoCapture::capture, this);

	return true;
}

void VideoCapture::exit()
{
	if(_isInitialized)
    {      
		_isInitialized = false;
		_thread.join();
		if (_capture->isStarted())
			stop();
        _capture->shutdown();
    }
}

bool VideoCapture::start()
{
	if (!_isInitialized)
	{
		return false;
	}

	if (0 != _capture->start()) 
	{
		return false;
	}

	return true;
}

void VideoCapture::stop()
{
	_capture->stop();
}

bool VideoCapture::getDisplaySetting(std::string& name)
{
	//DEVMODEW devMode;
	//EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &devMode);

	_width = GetSystemMetrics(SM_CXSCREEN);
	_height = GetSystemMetrics(SM_CYSCREEN);

	return true;
}

void VideoCapture::capture()
{
	while (_isInitialized)
	{
		_capture->update();	
		std::this_thread::sleep_for(std::chrono::milliseconds(15));
	}
}

void VideoCapture::FrameCallback(sc::PixelBuffer& buf)
{
    VideoCapture& vc = VideoCapture::instance();

	if (vc._frameBuffer->isFull())
	{
		return ;
	}

	int frameSize = buf.height * buf.width * 4;
	RGBAFrame frame(frameSize);
	frame.width = buf.width;
	frame.height = buf.height;
	memcpy(frame.data.get(), buf.plane[0], frameSize);
	vc._frameBuffer->push(std::move(frame));
}

bool VideoCapture::getFrame(RGBAFrame& frame)
{
	if (_frameBuffer->isEmpty() && _lastFrame.size!=0)
	{
		frame = _lastFrame;
		return true;
	}
	else
	{
		if (_frameBuffer->pop(_lastFrame))
		{
			frame = _lastFrame;
			return true;
		}
	}

	return false;
}
