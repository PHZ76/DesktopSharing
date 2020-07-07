#include "audio_capture.h"
#include "net/log.h"
#include "net/Timestamp.h"

AudioCapture::AudioCapture()
{
   
}

AudioCapture::~AudioCapture()
{
    
}

bool AudioCapture::Init(uint32_t buffer_size)
{
	if (is_initialized_) {
		return true;
	}

	if (capture_.init() < 0) {
		return false;
	}
	else {
		WAVEFORMATEX *audioFmt = capture_.getAudioFormat();
		channels_ = audioFmt->nChannels;
		samplerate_ = audioFmt->nSamplesPerSec;
		bits_per_sample_ = audioFmt->wBitsPerSample;
		player_.init();
	}

	audio_buffer_.reset(new AudioBuffer(buffer_size));

	if (StartCapture() < 0) {
		return false;
	}
	
	is_initialized_ = true;
	return true;
}

void AudioCapture::Destroy()
{
	if (is_initialized_) {
		StopCapture();
		is_initialized_ = false;
	}
}

int AudioCapture::StartCapture()
{
	capture_.setCallback([this](const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples) {
#if 0
		static xop::Timestamp tp;
		static int samplesPerSecond = 0;

		samplesPerSecond += samples;
		if (tp.elapsed() >= 990) {
			//printf("samples per second: %d\n", samplesPerSecond);
			samplesPerSecond = 0;
			tp.reset();
		}
#endif
		channels_ = mixFormat->nChannels;
		samplerate_ = mixFormat->nSamplesPerSec;
		bits_per_sample_ = mixFormat->wBitsPerSample;
		audio_buffer_->write((char*)data, mixFormat->nBlockAlign * samples);
	});

	audio_buffer_->clear();
	if (capture_.start() < 0) {
		return -1;
	}
	else {
		player_.start([this](const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples) {
			memset(data, 0, mixFormat->nBlockAlign*samples);
		});
	}

	is_started_ = true;
	return 0;
}

int AudioCapture::StopCapture()
{
	player_.stop();
	capture_.stop();
	is_started_ = false;
	return 0;
}

int AudioCapture::Read(uint8_t *data, uint32_t samples)
{
	if ((int)samples > this->GetSamples()) {
		return 0;
	}

	audio_buffer_->read((char*)data, samples * bits_per_sample_ / 8 * channels_);
	return samples;
}

int AudioCapture::GetSamples()
{
	return audio_buffer_->size() * 8 / bits_per_sample_ / channels_;
}
