#include "AudioCapture.h"
#include "net/log.h"
#include "net/Timestamp.h"

#pragma comment(lib, "portaudio_static_x86.lib")

#define PA_SAMPLE_TYPE  paInt16  
typedef short SAMPLE;

AudioCapture::AudioCapture()
	: _frameBuffer(new xop::RingBuffer<PCMFrame>(10))
{
    Pa_Initialize();
}

AudioCapture::~AudioCapture()
{
    Pa_Terminate();
}

AudioCapture& AudioCapture::instance()
{
	static AudioCapture s_ac;
	return s_ac;
}

bool AudioCapture::init(uint32_t samplerate, uint32_t channels)
{
	if (_isInitialized)
		return false;

	_inputParameters.device = Pa_GetDefaultInputDevice();
	if (_inputParameters.device == paNoDevice)
	{
		LOG("Default input device not found.\n");
		return false;
	}

	_inputParameters.channelCount = _channels;
	_inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	_inputParameters.suggestedLatency = Pa_GetDeviceInfo(_inputParameters.device)->defaultLowInputLatency;
	_inputParameters.hostApiSpecificStreamInfo = NULL;

	PaError error;
	error = Pa_OpenStream( &_stream, &_inputParameters, NULL, _samplerate,
						 AUDIO_LENGTH_PER_FRAME, paClipOff, FrameCallback, NULL);
	if (error != paNoError)
	{
		LOG("Pa_OpenStream() failed.");
	}

	_isInitialized = true;
	//_thread = std::thread(&AudioCapture::processEntries, this);

	return true;
}

void AudioCapture::exit()
{
	if (_isInitialized)
	{
		_isInitialized = false;
		if (isCapturing())
			stop();

		if (_stream)
		{
			Pa_CloseStream(_stream);
			_stream = nullptr;
		}
	}
}

int AudioCapture::FrameCallback(const void *inputBuffer, void *outputBuffer,
								unsigned long framesPerBuffer,
								const PaStreamCallbackTimeInfo* timeInfo,
								PaStreamCallbackFlags statusFlags,void *userData)
{
#if 0
	static xop::Timestamp tp;
	static int fps = 0;
	fps++;
	if (tp.elapsed() > 1000)
	{
		printf("audio fps: %d\n", fps);
		fps = 0;
		tp.reset();
	}
#endif

    AudioCapture& ac = AudioCapture::instance();
	if (ac._frameBuffer->isFull())
	{
		return paContinue;
	}

	int frameSize = ac._channels * sizeof(SAMPLE) * framesPerBuffer;
	PCMFrame frame(frameSize);
	memcpy(frame.data.get(), inputBuffer, frameSize);
	ac._frameBuffer->push(std::move(frame));
    return paContinue;
}

bool AudioCapture::start()
{
	if (isCapturing())
	{
		return false;
	}
    
	Pa_StartStream(_stream);
	return true;
}

void AudioCapture::stop()
{
	if(isCapturing())
    {
		Pa_StopStream(_stream);
    }
}

bool AudioCapture::getFrame(PCMFrame& frame)
{
	if (_frameBuffer->isEmpty())
	{
		return false;
	}

	return _frameBuffer->pop(frame);
}
