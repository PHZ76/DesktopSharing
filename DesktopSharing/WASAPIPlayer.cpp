#include "WASAPIPlayer.h"

WASAPIPlayer::WASAPIPlayer()
{

}

WASAPIPlayer::~WASAPIPlayer()
{

}

int WASAPIPlayer::init()
{
	std::lock_guard<std::mutex> locker(m_mutex);
	if (m_initialized)
	{
		return 0;
	}

	CoInitialize(NULL);

	HRESULT hr = S_OK;
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)m_enumerator.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to create instance.\n");
		return -1;
	}

	hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, m_device.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to create device.\n");
		return -1;
	}

	hr = m_device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)m_audioClient.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to activate device.\n");
		return -1;
	}

	hr = m_audioClient->GetMixFormat(&m_mixFormat);
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to get mix format.\n");
		return -1;
	}

	adjustFormatTo16Bits(m_mixFormat);
	m_hnsActualDuration = REFTIMES_PER_SEC;
	hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, m_hnsActualDuration, 0, m_mixFormat, NULL);
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to initialize audio client.\n");
		return -1;
	}

	hr = m_audioClient->GetBufferSize(&m_bufferFrameCount);
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to get buffer size.\n");
		return -1;
	}

	hr = m_audioClient->GetService(IID_IAudioRenderClient, (void**)m_audioRenderClient.GetAddressOf());
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to get service.\n");
		return -1;
	}

	// Calculate the actual duration of the allocated buffer.
	m_hnsActualDuration = (double)REFTIMES_PER_SEC * m_bufferFrameCount / m_mixFormat->nSamplesPerSec;

	m_initialized = true;
	return 0;
}

int WASAPIPlayer::exit()
{

	return 0;
}

int WASAPIPlayer::adjustFormatTo16Bits(WAVEFORMATEX *pwfx)
{
	if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
	{
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
	}
	else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>(pwfx);
		if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat))
		{
			pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			pEx->Samples.wValidBitsPerSample = 16;
		}
	}
	else
	{
		return -1;
	}

	pwfx->wBitsPerSample = 16;
	pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
	pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
	return 0;
}

int WASAPIPlayer::start(AudioDataCallback callback)
{
	std::lock_guard<std::mutex> locker(m_mutex);
	
	if (!m_initialized)
	{
		return -1;
	}

	if (m_isEnabeld)
	{
		return 0;
	}

	m_callback = callback;

	HRESULT hr = m_audioClient->Start();
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to start audio client.\n");
		return -1;
	}

	m_isEnabeld = true;
	m_threadPtr.reset(new std::thread([this] {
		while (this->m_isEnabeld)
		{
			if (this->play() < 0)
			{
				break;
			}
		}
	}));

	return 0;
}

int WASAPIPlayer::stop()
{
	std::lock_guard<std::mutex> locker(m_mutex);

	if (m_isEnabeld)
	{
		m_isEnabeld = false;
		m_threadPtr->join();

		HRESULT hr = m_audioClient->Stop();
		if (FAILED(hr))
		{
			printf("[WASAPIPlayer] Failed to stop audio client.\n");
			return -1;
		}
	}

	return 0;
}

int WASAPIPlayer::play()
{
	uint32_t numFramesPadding = 0;
	uint32_t numFramesAvailable = 0;

	HRESULT hr = m_audioClient->GetCurrentPadding(&numFramesPadding);
	if (FAILED(hr))
	{
		printf("[WASAPIPlayer] Failed to get current padding.\n");
		return -1;
	}

	numFramesAvailable = m_bufferFrameCount - numFramesPadding;

	if (numFramesAvailable > 0)
	{
		BYTE *data;
		hr = m_audioRenderClient->GetBuffer(numFramesAvailable, &data);
		if (FAILED(hr))
		{
			printf("[WASAPIPlayer] Audio render client failed to get buffer.\n");
			return -1;
		}

		if (m_callback)
		{
			memset(data, 0, m_mixFormat->nBlockAlign * numFramesAvailable);
			m_callback(m_mixFormat, data, numFramesAvailable);
		}

		hr = m_audioRenderClient->ReleaseBuffer(numFramesAvailable, 0);
		if (FAILED(hr))
		{
			printf("[WASAPIPlayer] Audio render client failed to release buffer.\n");
			return -1;
		}
	}
	else
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	return 0;
}