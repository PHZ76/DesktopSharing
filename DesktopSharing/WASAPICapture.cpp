#include "WASAPICapture.h"

WASAPICapture::WASAPICapture()
	: m_initialized(false)
	, m_isEnabeld(false)
	, m_mixFormat(NULL)
{
	m_pcmBufSize = 48000 * 32 * 2 * 2;
	m_pcmBuf.reset(new uint8_t[m_pcmBufSize]);
}

WASAPICapture::~WASAPICapture()
{
	if (m_initialized)
	{
		CoUninitialize();
	}
}

int WASAPICapture::init()
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
		printf("[WASAPICapture] Failed to create instance.\n");
		return -1;
	}

	hr = m_enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, m_device.GetAddressOf());
	if (FAILED(hr)) 
	{
		printf("[WASAPICapture] Failed to create device.\n");
		return -1;
	}

	hr = m_device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)m_audioClient.GetAddressOf());
	if (FAILED(hr)) 
	{
		printf("[WASAPICapture] Failed to activate device.\n");
		return -1;
	}

	hr = m_audioClient->GetMixFormat(&m_mixFormat);
	if (FAILED(hr)) 
	{
		printf("[WASAPICapture] Failed to get mix format.\n");
		return -1;
	}

	adjustFormatTo16Bits(m_mixFormat);
	m_hnsActualDuration = REFTIMES_PER_SEC;
	hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, m_hnsActualDuration, 0, m_mixFormat, NULL);
	if (FAILED(hr)) 
	{
		printf("[WASAPICapture] Failed to initialize audio client.\n");
		return -1;
	}

	hr = m_audioClient->GetBufferSize(&m_bufferFrameCount);
	if (FAILED(hr)) 
	{
		printf("[WASAPICapture] Failed to get buffer size.\n");
		return -1;
	}

	hr = m_audioClient->GetService(IID_IAudioCaptureClient, (void**)m_audioCaptureClient.GetAddressOf());
	if (FAILED(hr)) 
	{
		printf("[WASAPICapture] Failed to get service.\n");
		return -1;
	}

	// Calculate the actual duration of the allocated buffer.
	m_hnsActualDuration = REFERENCE_TIME(REFTIMES_PER_SEC * m_bufferFrameCount / m_mixFormat->nSamplesPerSec);

	m_initialized = true;
	return 0;
}

int WASAPICapture::exit()
{
	return 0;
}

int WASAPICapture::adjustFormatTo16Bits(WAVEFORMATEX *pwfx)
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

int WASAPICapture::start()
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

	HRESULT hr = m_audioClient->Start();
	if (FAILED(hr))
	{
		printf("[WASAPICapture] Failed to start audio client.\n");
		return -1;
	}
	
	m_isEnabeld = true;
	m_threadPtr.reset(new std::thread([this] {
		while (this->m_isEnabeld)
		{			
			if (this->capture() < 0)
			{
				break;
			}
		}
	}));

	return 0;
}

int WASAPICapture::stop()
{
	std::lock_guard<std::mutex> locker(m_mutex);
	if (m_isEnabeld)
	{
		m_isEnabeld = false;
		m_threadPtr->join();

		HRESULT hr = m_audioClient->Stop();
		if (FAILED(hr))
		{
			printf("[WASAPICapture] Failed to stop audio client.\n");
			return -1;
		}
	}

	return 0;
}

void WASAPICapture::setCallback(PacketCallback callback)
{
	std::lock_guard<std::mutex> locker(m_mutex2);
	m_callback = callback;
}

int WASAPICapture::capture()
{
	HRESULT hr = S_OK;
	uint32_t packetLength = 0;
	uint32_t numFramesAvailable = 0;
	BYTE *pData;
	DWORD flags;

	hr = m_audioCaptureClient->GetNextPacketSize(&packetLength);
	if (FAILED(hr)) 
	{
		printf("[WASAPICapture] Faild to get next data packet size.\n");
		return -1;
	}

	if (packetLength == 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		return 0;
	}

	while (packetLength != 0) 
	{
		hr = m_audioCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
		if (FAILED(hr)) 
		{
			printf("[WASAPICapture] Faild to get buffer.\n");
			return -1;
		}

		if (m_pcmBufSize < numFramesAvailable * m_mixFormat->nBlockAlign)
		{
			m_pcmBufSize = numFramesAvailable * m_mixFormat->nBlockAlign;
			m_pcmBuf.reset(new uint8_t[m_pcmBufSize]);
		}

		if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
		{
			memset(m_pcmBuf.get(), 0, m_pcmBufSize);
		}
		else
		{
			memcpy(m_pcmBuf.get(), pData, numFramesAvailable * m_mixFormat->nBlockAlign);
		}

		{
			std::lock_guard<std::mutex> locker(m_mutex2);
			if (m_callback)
			{
				m_callback(m_mixFormat, pData, numFramesAvailable);
			}
		}

		hr = m_audioCaptureClient->ReleaseBuffer(numFramesAvailable);
		if (FAILED(hr)) 
		{
			printf("[WASAPICapture] Faild to release buffer.\n");
			return -1;
		}

		hr = m_audioCaptureClient->GetNextPacketSize(&packetLength);
		if (FAILED(hr)) 
		{
			printf("[WASAPICapture] Faild to get next data packet size.\n");
			return -1;
		}
	}

	return 0;
}