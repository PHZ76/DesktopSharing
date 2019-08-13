#pragma once

#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <wrl.h>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <mutex>
#include <memory>
#include <thread>


class WASAPIPlayer
{
public:
	typedef std::function<void(const WAVEFORMATEX *mixFormat, uint8_t *data, uint32_t samples)> AudioDataCallback;

	WASAPIPlayer();
	~WASAPIPlayer();
	WASAPIPlayer &operator=(const WASAPIPlayer &) = delete;
	WASAPIPlayer(const WASAPIPlayer &) = delete;

	int init();
	int exit();
	int start(AudioDataCallback callback);
	int stop();

private:
	int adjustFormatTo16Bits(WAVEFORMATEX *pwfx);
	int play();

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID IID_IAudioClient = __uuidof(IAudioClient);
	const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
	const int REFTIMES_PER_SEC = 10000000;
	const int REFTIMES_PER_MILLISEC = 10000;

	bool m_initialized;
	bool m_isEnabeld;
	std::mutex m_mutex;
	std::shared_ptr<std::thread> m_threadPtr;
	WAVEFORMATEX *m_mixFormat;
	REFERENCE_TIME m_hnsActualDuration;
	uint32_t m_bufferFrameCount;
	AudioDataCallback m_callback;
	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> m_enumerator;
	Microsoft::WRL::ComPtr<IMMDevice> m_device;
	Microsoft::WRL::ComPtr<IAudioClient> m_audioClient;
	Microsoft::WRL::ComPtr<IAudioRenderClient> m_audioRenderClient;
};

