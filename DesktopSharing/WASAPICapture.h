// https://docs.microsoft.com/en-us/previous-versions//ms678709(v=vs.85)

#ifndef WASAPI_CAPTURE_H
#define WASAPI_CAPTURE_H

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl.h>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <mutex>
#include <memory>
#include <thread>

class WASAPICapture
{
public:
	typedef std::function<void(const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples)> PacketCallback;

	WASAPICapture();
	~WASAPICapture();
	WASAPICapture &operator=(const WASAPICapture &) = delete;
	WASAPICapture(const WASAPICapture &) = delete;

	int init();
	int exit();
	int start();
	int stop();
	void setCallback(PacketCallback callback);

	WAVEFORMATEX *getAudioFormat() const
	{
		return m_mixFormat;
	}

private:
	int adjustFormatTo16Bits(WAVEFORMATEX *pwfx);
	int capture();

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID IID_IAudioClient = __uuidof(IAudioClient);
	const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
	const int REFTIMES_PER_SEC = 10000000;
	const int REFTIMES_PER_MILLISEC = 10000;

	bool m_initialized;
	bool m_isEnabeld;
	std::mutex m_mutex, m_mutex2;
	std::shared_ptr<std::thread> m_threadPtr;
	WAVEFORMATEX *m_mixFormat;
	REFERENCE_TIME m_hnsActualDuration;
	uint32_t m_bufferFrameCount;
	PacketCallback m_callback;
	std::shared_ptr<uint8_t> m_pcmBuf;
	uint32_t m_pcmBufSize;
	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;
	Microsoft::WRL::ComPtr<IMMDevice> m_device;
	Microsoft::WRL::ComPtr<IAudioClient> m_audioClient;
	Microsoft::WRL::ComPtr<IAudioCaptureClient> m_audioCaptureClient;
};

#endif