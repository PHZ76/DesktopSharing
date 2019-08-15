#include "AudioCapture.h"
#include "net/log.h"
#include "net/Timestamp.h"

AudioCapture::AudioCapture()
{
   
}

AudioCapture::~AudioCapture()
{
    
}

AudioCapture& AudioCapture::instance()
{
	static AudioCapture s_ac;
	return s_ac;
}

int AudioCapture::init()
{
	if (m_isInitialized)
	{
		return -1;
	}

	if (m_capture.init() < 0)
	{
		return -1;
	}
	else
	{
		WAVEFORMATEX *audioFmt = m_capture.getAudioFormat();
		m_channels = audioFmt->nChannels;
		m_samplerate = audioFmt->nSamplesPerSec;
		m_bitsPerSample = audioFmt->wBitsPerSample;
		m_player.init();
	}

	m_isInitialized = true;
	return 0;
}

int AudioCapture::exit()
{
	if (m_isInitialized)
	{
		if (m_isEnabled)
		{
			stop();
		}
	}

	return 0;
}

int AudioCapture::start()
{
	if (!m_isInitialized)
	{
		return -1;
	}

	if (m_isEnabled)
	{
		return 0;
	}

	m_capture.setCallback([this](const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples) {

		static xop::Timestamp tp;
		static int samplesPerSecond = 0;

		samplesPerSecond += samples;
		if (tp.elapsed() >= 990)
		{
			//printf("samples per second: %d\n", samplesPerSecond);
			samplesPerSecond = 0;
			tp.reset();
		}

		m_channels = mixFormat->nChannels;
		m_samplerate = mixFormat->nSamplesPerSec;
		m_bitsPerSample = mixFormat->wBitsPerSample;
		m_audioBuffer.write((char*)data, mixFormat->nBlockAlign * samples);
	});

	m_audioBuffer.clear();
	if (m_capture.start() < 0)
	{
		return -1;
	}
	else
	{
		m_player.start([this](const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples) {
			memset(data, 0, mixFormat->nBlockAlign*samples);
		});
	}

	m_isEnabled = true;
	return 0;
}

int AudioCapture::stop()
{
	if (!m_isInitialized)
	{
		return -1;
	}

	if (m_isEnabled)
	{
		m_player.stop();
		m_capture.stop();
		m_isEnabled = false;
	}

	return 0;
}

int AudioCapture::readSamples(uint8_t *data, uint32_t samples)
{
	if ((int)samples > this->getSamples())
	{
		return 0;
	}

	m_audioBuffer.read((char*)data, samples * m_bitsPerSample / 8 * m_channels);
	return samples;
}

int AudioCapture::getSamples()
{
	return m_audioBuffer.size() * 8 / m_bitsPerSample / m_channels;
}

int AudioCapture::getSamplerate()
{
	return m_samplerate;
}

int AudioCapture::getChannels()
{
	return m_channels;
}

int AudioCapture::getBitsPerSample()
{
	return m_bitsPerSample;
}