#ifndef _AUDIO_CAPTURE_H
#define _AUDIO_CAPTURE_H

#include <thread>
#include <cstdint>
#include <memory>
#include "WASAPICapture.h"
#include "WASAPIPlayer.h"
#include "AudioBuffer.h"

class AudioCapture
{
public:
	AudioCapture & operator=(const AudioCapture &) = delete;
	AudioCapture(const AudioCapture &) = delete;
	static AudioCapture& instance();
	~AudioCapture();

	int init();
	int exit();
	int start();
	int stop();
	
	int readSamples(uint8_t*data,uint32_t samples);

	int getSamples();

	int getSamplerate();

	int getChannels();

	int getBitsPerSample();

	bool isCapturing() const 
	{ return m_isEnabled; }

private:
	AudioCapture();
	
	bool m_isInitialized = false;
	bool m_isEnabled = false;

	uint32_t m_channels = 2;
	uint32_t m_samplerate = 48000;
	uint32_t m_bitsPerSample = 16;

	WASAPIPlayer m_player;
	WASAPICapture m_capture;
	AudioBuffer m_audioBuffer;
};

#endif