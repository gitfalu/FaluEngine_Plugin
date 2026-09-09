#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include <string>
#include "AudioClip.h"
#include <FaluEngine/AudioTypes.h>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	using Microsoft::WRL::ComPtr;

	class FALU_ENGINE_API AudioEngine
	{
	private:
		struct ActiveVoice : public IXAudio2VoiceCallback
		{
			IXAudio2SourceVoice* voice = nullptr;
			bool finished = false;

			void __stdcall OnStreamEnd() noexcept override { finished = true; }
			void __stdcall OnVoiceProcessingPassEnd() noexcept override {}
			void __stdcall OnVoiceProcessingPassStart(UINT32) noexcept override {}
			void __stdcall OnBufferStart(void*) noexcept override {}
			void __stdcall OnBufferEnd(void*) noexcept override {}
			void __stdcall OnLoopEnd(void*) noexcept override {}
			void __stdcall OnVoiceError(void*, HRESULT) noexcept override {}

			~ActiveVoice()
			{
				if (voice)
				{
					voice->Stop();
					voice->DestroyVoice();
				}
			}
		};


	public:
		static AudioEngine& get();

		AudioEngine(const AudioEngine&) = delete;
		AudioEngine& operator=(const AudioEngine&) = delete;

		bool init();
		void shutdown();

		void update();

		AudioVoiceHandle play(const std::string& path, float volume = 1.0f, bool loop = false);

		void stop(AudioVoiceHandle& handle);
		void setVolume(AudioVoiceHandle& handle, float volume);

		void setMasterVolume(float volume);

	private:
		AudioEngine() = default;
		~AudioEngine() { shutdown(); }

		ComPtr<IXAudio2> m_xaudio2;
		IXAudio2MasteringVoice* m_masteringVoice = nullptr;

		std::vector<std::unique_ptr<ActiveVoice>> m_activeVoice;

		bool m_initialized = false;
	};
}
