#ifndef SCREEN_CAPTURE_H
#define SCREEN_CAPTURE_H

#include <cstdint>
#include <vector>

class ScreenCapture
{
public:
	ScreenCapture & operator=(const ScreenCapture &) = delete;
	ScreenCapture(const ScreenCapture &) = delete;
	ScreenCapture() {}
	virtual ~ScreenCapture() {}

	virtual bool Init() = 0;
	virtual bool Destroy() = 0;

	virtual bool CaptureFrame(std::vector<uint8_t>& image, uint32_t& width, uint32_t& height) = 0;

	virtual uint32_t GetWidth()  const = 0;
	virtual uint32_t GetHeight() const = 0;
	virtual bool CaptureStarted() const = 0;

protected:

};

#endif