#include "AudioEngine.h"
#include "asset/AssetManager.h"
#include "core/Logger.h"
#include <algorithm>

namespace FaluEngine
{
	bool AudioEngine::init()
	{
		if (m_initialized) return true;

		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		{
			LOG_ERROR("AudioEngine: CoInitializeEx failed (hr={:#x})", static_cast<unsigned>(hr));
			return false;
		}

		hr = XAudio2Create(&m_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
		if (FAILED(hr))
		{
			LOG_ERROR("AudioEngine: XAudio2Create failed (hr={:#x})", static_cast<unsigned>(hr));
			return false;
		}

		hr = m_xaudio2->CreateMasteringVoice(&m_masteringVoice);
		if (FAILED(hr))
		{
			LOG_ERROR("AudioEngine: CreateMasteringVoice failed (hr={:#x})", static_cast<unsigned>(hr));
			return false;
		}

		AssetManager::get().registerLoader<AudioClip>(
			[](const std::string& path) -> std::shared_ptr<AudioClip> {
				return AudioClip::load(path);
			});

		m_initialized = true;
		LOG_INFO("AudioEngine initialized");
		return true;
	}

	void AudioEngine::shutdown()
	{
		if (!m_initialized) return;

		m_activeVoice.clear();

		if (m_masteringVoice)
		{
			m_masteringVoice->DestroyVoice();
			m_masteringVoice = nullptr;
		}
		m_xaudio2.Reset();

		m_initialized = false;
	}

	void AudioEngine::update()
	{
		m_activeVoice.erase(
			std::remove_if(m_activeVoice.begin(), m_activeVoice.end(),
				[](const std::unique_ptr<ActiveVoice>& v) { return v->finished; }),
			m_activeVoice.end()
		);
	}

	AudioVoiceHandle AudioEngine::play(const std::string& path, float volume, bool loop)
	{
		AudioVoiceHandle handle;
		if (!m_initialized)
		{
			LOG_WARN("AudioEngine::play called before init()");
			return handle;
		}

		auto clip = AssetManager::get().load<AudioClip>(path);
		if (!clip || !clip->loaded)
		{
			LOG_ERROR("AudioEngine::play failed to load clip: {}", path);
			return handle;
		}

		auto active = std::make_unique<ActiveVoice>();

		HRESULT hr = m_xaudio2->CreateSourceVoice(
			&active->voice, &clip->format, 0,
			XAUDIO2_DEFAULT_FREQ_RATIO, active.get()
		);
		if (FAILED(hr))
		{
			LOG_ERROR("AudioEngine: CreateSourceVoice failed (hr={:#x})", static_cast<unsigned>(hr));
			return handle;
		}

		XAUDIO2_BUFFER buffer{};
		buffer.AudioBytes = static_cast<UINT32>(clip->pcmData.size());
		buffer.pAudioData = clip->pcmData.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

		active->voice->SetVolume(volume);

		hr = active->voice->SubmitSourceBuffer(&buffer);
		if (FAILED(hr))
		{
			LOG_ERROR("AudioEngine: SubmitSourceBuffer failed (hr={:#x})", static_cast<unsigned>(hr));
			return handle;
		}

		active->voice->Start();

		handle.voice = active->voice;
		m_activeVoice.push_back(std::move(active));

		return handle;
	}

	void AudioEngine::stop(AudioVoiceHandle& handle)
	{
		if (!handle.voice) return;

		auto it = std::find_if(m_activeVoice.begin(), m_activeVoice.end(),
			[&](const std::unique_ptr<ActiveVoice>& v) {return v->voice == handle.voice; }
		);
		if (it != m_activeVoice.end())
		{
			m_activeVoice.erase(it);
		}
		handle.voice = nullptr;
	}

	void AudioEngine::setVolume(AudioVoiceHandle& handle, float volume)
	{
		if (handle.voice) handle.voice->SetVolume(volume);
	}

	void AudioEngine::setMasterVolume(float volume)
	{
		if (m_masteringVoice) m_masteringVoice->SetVolume(volume);
	}
}

