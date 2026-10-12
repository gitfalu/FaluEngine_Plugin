/*****************************************************************//**
 * \file   HotReloadManager.h
 * \brief  C++とHLSLのホットリロード管理。エディタのウィンドウがアクティブになった時に変更感知でビルドを通しDLL差し替え
 * 
 * \author tsunn
 * \date   October 2026
 *********************************************************************/
#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class FALU_ENGINE_API HotReloadManager
	{
	public:
		struct Config
		{
			// C++
			std::filesystem::path cmakeExe; // Cmake.exe
			std::filesystem::path buildDir; // Cmakeのビルドディレクトリ
			std::string buildConfig = "Debug"; // マルチコンフィグ用
			std::string target = "GameCode";
			bool skipDependencyBuild = false;

			std::vector<std::filesystem::path> sourceDirs; // 監視するソースフォルダ
			std::filesystem::path dllPath; // ビルド成果物のパス

			// HLSL
			std::filesystem::path shaderSourceDir; // リポジトリ側のパス
			std::filesystem::path shaderRuntimeDir; // 実行時のパス
		};

		struct Hooks
		{
			std::function<void()> beforeUnload; // DLLから参照を持っているオブジェクトから参照を破棄
			std::function<void()> afterLoad; // 再ロード後にスクリプトを再バインドする
		};

		/// @brief ビルド実行状態
		enum class State { Idle,Building,BuildFailed,PendingReload,LoadFailed};

		static HotReloadManager& get();

		void setHooks(Hooks hooks) { m_hooks = std::move(hooks); }

		/// @brief 初期化処理。DLLが存在すればロード、ソースが新しければビルドも行う
		/// @param cfg 
		void init(Config cfg);
		void shutdown();

		/// @brief ウィンドウがアクティブになった時に処理を流す関数
		void onEditorFocused();

		/// @brief 手動リビルド
		void requestRebuild();

		/// @brief 毎フレーム更新
		/// @param canReload 
		void update(bool canReload);

		/// @brief レンダラーのシェーダー再読み込みを呼ぶか判断する関数
		/// @return trueでシェーダー再読み込みをかける
		[[nodiscard]] bool consumeShaderReloadRequest() { return m_shaderReload.exchange(false); }

		[[nodiscard]] State getState() const noexcept { return m_state; }
		[[nodiscard]] bool isBusy() const noexcept { return m_state == State::Building; }
		/// @brief 現在のビルド状況をテキストとして受け渡す
		/// @return 
		[[nodiscard]] std::string statusText() const;
		[[nodiscard]] std::vector<std::string> getBuildLog()const;

	private:
		HotReloadManager() = default;

		void startBuild();
		void finishBuild();
		bool reloadNow();
		void cleanupShadows(const std::filesystem::path& keep);
		std::filesystem::file_time_type newestSourceTime(bool cmakeOnly = false) const;
		bool syncShaders();

		Config m_cfg;
		Hooks m_hooks;
		bool m_initialized = false;

		State m_state = State::Idle;
		std::filesystem::file_time_type m_buildStamp{}; // 最後にビルドを開始した時点のソース最新時刻
		std::filesystem::file_time_type m_shaderStamp{};
		std::string m_loadedPath;
		uint32_t m_generation = 0;
		bool m_rebuildForced = false;
		std::filesystem::file_time_type m_builtCMakeStamp{};
		std::atomic<bool> m_sawError{ false };
		std::filesystem::path m_pendingPdb;
		std::filesystem::path m_currentPdb;
		std::string m_failHint;

		// BuildProcess
		std::thread m_thread;
		void* m_process = nullptr;
		std::atomic<bool> m_buildDone{ false }; // atomic : マルチスレッドでの分割処理をしないようにする
		std::atomic<int> m_exitCode{ 0 };
		mutable std::mutex m_logMutex;
		std::vector<std::string> m_log; // ビルドログ全行
		std::vector<std::string> m_pendingErrors;
		std::atomic<bool> m_shaderReload{ false };
	};
}
