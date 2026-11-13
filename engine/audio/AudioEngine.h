/*****************************************************************//**
 * \file   AudioEngine.h
 * \brief  音声の再生や停止など音声再生に関する処理を行うスクリプトヘッダ
 * 
 * \author tsunn
 * \date   November 2026
 *********************************************************************/
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
	using Microsoft::WRL::ComPtr; // 参照がなくなった時に自動的に解放されるスマートポインタ

	/// @brief オーディオの再生や停止などの実行状態を管理する関数
	class FALU_AUDIO_API AudioEngine
	{
	private:
		/// @brief XAudio2を継承したオーディオデータ　：https://learn.microsoft.com/ja-jp/windows/win32/xaudio2/xaudio2-introduction
		struct ActiveVoice : public IXAudio2VoiceCallback
		{
			IXAudio2SourceVoice* voice = nullptr;
			bool finished = false;
			// DLL出力時に標準出力を保つために標準呼び出し規約を使用する
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
		/// @brief コピー禁止コンストラクタ
		/// @param  
		AudioEngine(const AudioEngine&) = delete;
		AudioEngine& operator=(const AudioEngine&) = delete;

		/// @brief 初期化処理
		/// @return 初期化成功フラグ
		bool init();
		/// @brief 終了処理
		void shutdown();
		/// @brief 更新処理
		void update();

		/// @brief 音声をファイルパスから再生する関数
		/// @param path ファイルパス
		/// @param volume 音量(デフォルトでは1.0f(100%))
		/// @param loop ループフラグ
		/// @return 
		AudioVoiceHandle play(const std::string& path, float volume = 1.0f, bool loop = false);

		void stop(AudioVoiceHandle& handle);

		/// @brief  音声ごとの音量を設定する
		/// @param handle 音声のハンドル
		/// @param volume 音量(0.0f~1.0f)
		void setVolume(AudioVoiceHandle& handle, float volume);

		/// @brief 全体の音量を設定する
		/// @param volume 音量
		void setMasterVolume(float volume);

	private:
		/// @brief デフォルトコンストラクタ
		AudioEngine() = default;
		/// @brief デストラクタ
		~AudioEngine() { shutdown(); }

		ComPtr<IXAudio2> m_xaudio2;
		/// @brief 主音声
		IXAudio2MasteringVoice* m_masteringVoice = nullptr;
		/// @brief 再生中の音声の動的配列
		std::vector<std::unique_ptr<ActiveVoice>> m_activeVoice;

		bool m_initialized = false;
	};
}
