#ifndef _AUDIO_CAPTURE_H
#define _AUDIO_CAPTURE_H

#include <thread>
#include <cstdint>
#include <memory>
#include "xop/RingBuffer.h"
#include "portaudio.h"  

#define AUDIO_LENGTH_PER_FRAME 1024

struct PCMFrame
{
	PCMFrame(uint32_t size = 100)
		: data(new uint8_t[size + 1024])
	{
		this->size = size;
	}
	uint32_t size = 0;
	uint32_t channels = 2;
	uint32_t samplerate = 44100;
	std::shared_ptr<uint8_t> data;
};

class AudioCapture
{
public:
	AudioCapture & operator=(const AudioCapture &) = delete;
	AudioCapture(const AudioCapture &) = delete;
	static AudioCapture& Instance();
	~AudioCapture();

	bool Init(uint32_t samplerate=44100, uint32_t channels = 2);
	void Exit();

	bool Start();
	void Stop();
	
	bool getFrame(PCMFrame& frame);

	bool isCapturing()
	{
		return (_isInitialized && Pa_IsStreamActive(_stream));
	}

private:
	AudioCapture();
	static int FrameCallback(const void *inputBuffer, void *outputBuffer,
							unsigned long framesPerBuffer,
							const PaStreamCallbackTimeInfo* timeInfo,
							PaStreamCallbackFlags statusFlags, void *userData);

	PaStreamParameters _inputParameters;
	PaStream* _stream = nullptr;
	bool _isInitialized = false;
	uint32_t _channels = 2;
	uint32_t _samplerate = 44100;
	std::shared_ptr<RingBuffer<PCMFrame>> _frameBuffer;
};

#endif