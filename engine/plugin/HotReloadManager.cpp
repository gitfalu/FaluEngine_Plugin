#include "HotReloadManager.h"
#include "PluginManager.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "core/Logger.h"
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif // !NOMINMAX
#include <Windows.h>

namespace fs = std::filesystem;

namespace FaluEngine
{
	namespace
	{
		// MSVC/Cmakeの出力はANSIコードページ。UTF-8に直す
		std::string ansiToUtf8(const std::string& s)
		{
			if (s.empty()) return {};
			// 文字列がUtf8に変換できるか確認。変換できたら変換したものを返す
			if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0) > 0)
				return s;
			// 文字列がAnsiに変換できるか確認。変換できなかったら元の文字列を返す
			int wlen = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
			if (wlen <= 0)return s;
			std::wstring w(wlen, L'\0');
			// 文字列がAnsiに変換できるか確認。変換でき無かったら元の文字列を返す
			MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), w.data(), wlen);
			int ulen = WideCharToMultiByte(CP_UTF8, 0, w.data(), wlen, nullptr, 0, nullptr, nullptr);
			if (ulen <= 0) return s;
			std::string u(ulen, L'\0');
			// 文字列がUtf8に変換できるか確認。変換できたら変換したものを返す
			WideCharToMultiByte(CP_UTF8, 0, w.data(), wlen, u.data(), ulen, nullptr, nullptr);
			return u;
		}

		bool isSourceFile(const fs::path& p)
		{
			const auto ext = p.extension().string();
			return ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".inl" || ext == "cc" ||
				p.filename() == "CMakeLists.txt";
		}

		/// @brief エラーを出力している行があるかを確認する
		/// @param l 
		/// @return 
		bool isErrorLine(const std::string& l)
		{
			return l.find(": error ") != std::string::npos ||
				l.find("fatal error") != std::string::npos ||
				l.find("error LNK") != std::string::npos ||
				l.find("CMake Error") != std::string::npos;
		}
	}

	HotReloadManager& HotReloadManager::get()
	{
		static HotReloadManager instance;
		return instance;
	}

	void HotReloadManager::init(Config cfg)
	{
		m_cfg = std::move(cfg);
		m_initialized = true;
		m_builtCMakeStamp = newestSourceTime(true);

		// 前回軌道の残骸を掃除
		cleanupShadows({});

		// DLLがすでにあればロード
		std::error_code ec;
		if (fs::exists(m_cfg.dllPath, ec))
		{
			if (!reloadNow())
			{
				LOG_WARN("Hotreload: initial GameCode load failed");
			}
		}

		// 最終更新の時間を取得する
		const auto src = newestSourceTime();
		const auto dll = fs::exists(m_cfg.dllPath, ec) ? fs::last_write_time(m_cfg.dllPath, ec)
			: fs::file_time_type::min();
		// 差分があればビルドをかける
		if (src > dll)
			startBuild();
		else
			m_buildStamp = src;

		m_shaderStamp = fs::file_time_type::min();
		syncShaders();
		m_shaderReload = false;
	}

	void HotReloadManager::shutdown()
	{
		if (!m_initialized) return;
		if (m_process) TerminateProcess(static_cast<HANDLE>(m_process), 1);
		if (m_thread.joinable()) m_thread.join();
		if (!m_loadedPath.empty())
		{
			PluginManager::get().unload(m_loadedPath);
			m_loadedPath.clear();
		}
		m_initialized = false;
	}

	void HotReloadManager::onEditorFocused()
	{
		if (!m_initialized) return;

		// HLSL
		if (syncShaders()) m_shaderReload = true;

		// C++
		if (m_state == State::Building) return;
		if (newestSourceTime() > m_buildStamp)
		{
			startBuild();
		}
	}

	void HotReloadManager::requestRebuild()
	{
		if (!m_initialized || m_state == State::Building) return;
		m_rebuildForced = true;
		startBuild();
	}

	void HotReloadManager::update(bool canReload)
	{
		if (!m_initialized) return;

		{
			std::lock_guard lk(m_logMutex);
			for (auto& l : m_pendingErrors) LOG_ERROR("[Build] {}", l);
			m_pendingErrors.clear();
		}

		if (m_state == State::Building && m_buildDone.load())
			finishBuild();

		if (m_state == State::PendingReload && canReload)
		{
			if (reloadNow()) m_state = State::Idle;
			else m_state = State::LoadFailed;
		}
	}

	std::string HotReloadManager::statusText() const
	{
		switch (m_state)
		{
		case FaluEngine::HotReloadManager::State::Building:
			return "Compiling C++...";
		case FaluEngine::HotReloadManager::State::BuildFailed:
			return "Build FAILED (see log)";
		case FaluEngine::HotReloadManager::State::PendingReload:
			return "Reload pending  (stop Play)";
		case FaluEngine::HotReloadManager::State::LoadFailed:
			return "GameCode load failed";
		default: return {};
		}
	}

	std::vector<std::string> HotReloadManager::getBuildLog() const
	{
		std::lock_guard lk(m_logMutex);
		return m_log;
	}

	void HotReloadManager::startBuild()
	{
		if (m_cfg.cmakeExe.empty() || m_cfg.buildDir.empty())
		{
			LOG_ERROR("hotReload: cmakeExe / buildDir not configured");
			return;
		}
		if (m_thread.joinable()) m_thread.join();

		// 最終更新日時を更新
		const auto cmakeStamp = newestSourceTime(true);
		const bool cmakeChanged = cmakeStamp > m_builtCMakeStamp;
		m_builtCMakeStamp = cmakeStamp;
		m_buildStamp = newestSourceTime();
		m_buildDone = false;
		m_exitCode = 0;
		{
			std::lock_guard lk(m_logMutex);
			m_log.clear();
			m_pendingErrors.clear();
		}

		SECURITY_ATTRIBUTES sa{ sizeof(sa),nullptr,TRUE };
		HANDLE readPipe = nullptr, writePipe = nullptr;
		if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
		{
			LOG_ERROR("HotReload: CraetePipe failed");
			return;
		}
		
		SetHandleInformation(readPipe,HANDLE_FLAG_INHERIT,0);
		
		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = writePipe;
		si.hStdError = writePipe;
		si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

		std::wstring cmd = L"\"" + m_cfg.cmakeExe.wstring() + L"\" --build \"" +
			m_cfg.buildDir.wstring() + L"\" --target " +
			std::wstring(m_cfg.target.begin(), m_cfg.target.end()) + 
			L" --config " + std::wstring(m_cfg.buildConfig.begin(),m_cfg.buildConfig.end());

		// CMakeList.txt
		const bool fullBuild = m_rebuildForced || cmakeChanged;
		if (!fullBuild && m_cfg.skipDependencyBuild)
			cmd += L" -- /p:BuildProjectReferences=false /nologo /v:m";

		// GameCode.pdb
		{
			std::error_code pec;
			const fs::path hotDir = m_cfg.dllPath.parent_path() / "hot";
			fs::create_directories(hotDir, pec);
			const auto tick = std::chrono::system_clock::now().time_since_epoch().count();
			m_pendingPdb = hotDir / (m_cfg.dllPath.stem().wstring() + L"_" + std::to_wstring(tick) + L".pdb");
			const std::wstring opt = L"/INCREMENTAL:NO /PDB:\"" + m_pendingPdb.wstring() + L"\"";
			SetEnvironmentVariableW(L"_LINK_", opt.c_str());
		}

		PROCESS_INFORMATION pi{};
		BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
			CREATE_NO_WINDOW, nullptr, m_cfg.buildDir.wstring().c_str(), &si, &pi);
		SetEnvironmentVariableW(L"_LINK_", nullptr);
		CloseHandle(writePipe);
		if (!ok)
		{
			CloseHandle(readPipe);
			LOG_ERROR("HtoReload: failed to launch cmake (error {})", GetLastError());
			return;
		}
		CloseHandle(pi.hThread);
		m_process = pi.hProcess;
		m_state = State::Building;
		LOG_INFO("HotReload: building {} ({})...", m_cfg.target, m_cfg.buildConfig);

		HANDLE proc = pi.hProcess;
		m_thread = std::thread([this, readPipe, proc]()
			{
				std::string buf;
				char chunk[4896];
				DWORD n = 0;
				auto flushLine = [this](std::string line)
					{
						while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
							line.pop_back();
						if (line.empty()) return;
						line = ansiToUtf8(line);
						std::lock_guard lk(m_logMutex);
						m_log.push_back(line);
						if (isErrorLine(line)) m_pendingErrors.push_back(line);
					};
				while (ReadFile(readPipe, chunk, sizeof(chunk), &n, nullptr) && n > 0)
				{
					buf.append(chunk, n);
					size_t pos;
					while ((pos = buf.find('\n')) != std::string::npos)
					{
						flushLine(buf.substr(0, pos)); buf.erase(0, pos + 1);
					}
				}
				flushLine(buf);
				CloseHandle(readPipe);

				WaitForSingleObject(proc, INFINITE);
				DWORD code = 1;
				GetExitCodeProcess(proc, &code);
				m_exitCode = static_cast<int>(code);
				m_buildDone = true;
			});
	}

	void HotReloadManager::finishBuild()
	{
		if (m_thread.joinable()) m_thread.join();
		if (m_process) { CloseHandle(static_cast<HANDLE>(m_process)); m_process = nullptr; }

		const int code = m_exitCode.load();
		if (code == 0)
		{
			LOG_INFO("HotReload: build succeeded");
			m_failHint.clear();
			m_currentPdb = m_pendingPdb;
			m_state = State::PendingReload;
		}
		else
		{
			bool engineLocked = false;
			std::vector<std::string> tail;
			{
				std::lock_guard lk(m_logMutex);
				for (auto& l : m_log)
				{
					if (l.find("LNK1104") != std::string::npos || l.find("LNK1168") != std::string::npos)
						engineLocked = true;
				}

				if (!m_sawError.load())
				{
					const size_t from = m_log.size() > 8 ? m_log.size() - 8 : 0;
					tail.assign(m_log.begin() + from, m_log.end());
				}
			}
			for (auto& l : tail) LOG_ERROR("[Build] {}", l);
			LOG_ERROR("HotReload: build FAILED (exit code {}). Old GameCode keeps running.", m_exitCode.load());

			if (engineLocked) m_failHint = "Engine DLL is locked: restart the editor";
			else if (!m_sawError.load()) m_failHint = "Build command failed (see log)";
			else m_failHint = "Build FAILED (see log)";

			if (!m_sawError.load())
			{
				m_buildStamp = std::filesystem::file_time_type::min();
				m_builtCMakeStamp = std::filesystem::file_time_type::min();
			}
			m_state = State::BuildFailed;
		}

		m_rebuildForced = false;
	}

	bool HotReloadManager::reloadNow()
	{
		std::error_code ec;
		if (!fs::exists(m_cfg.dllPath, ec))
		{
			LOG_ERROR("HotReload: DLL not found: {}", m_cfg.dllPath.string());
			return false;
		}

		const fs::path dir = m_cfg.dllPath.parent_path() / "hot";
		fs::create_directories(dir, ec);
		const fs::path shadow = dir / (m_cfg.dllPath.stem().string() + "_" + 
			std::to_string(++m_generation) + ".dll");
		fs::copy_file(m_cfg.dllPath, shadow, fs::copy_options::overwrite_existing, ec);
		if (ec)
		{
			LOG_ERROR("HotReload: shadow copy failed: {}", ec.message());
			return false;
		}

		if (m_hooks.beforeUnload) m_hooks.beforeUnload();
		if (!m_loadedPath.empty())
		{
			PluginManager::get().unload(m_loadedPath);
			m_loadedPath.clear();
		}

		const std::string shadowStr = shadow.string();
		const bool ok = PluginManager::get().load(shadowStr);
		if (ok)
		{
			m_loadedPath = shadowStr;
			if (auto* plugin = PluginManager::get().getPlugin(shadowStr))
				for (auto& [name, factory] : plugin->getScriptFactories())
					NativeScriptRegistry::get().registerScript(name, factory);
			LOG_INFO("HotReload: GameCode reloaded ({} scripts)",
				NativeScriptRegistry::get().getRegisteredNames().size());
		}
		else
		{
			LOG_ERROR("HotReload: failed to load {}", shadowStr);
		}

		if (m_hooks.afterLoad) m_hooks.afterLoad();

		cleanupShadows(shadow);
		return ok;
	}

	void HotReloadManager::cleanupShadows(const std::filesystem::path& keep)
	{
		std::error_code ec;
		const fs::path dir = m_cfg.dllPath.parent_path() / "hot";
		if (!fs::exists(dir, ec)) return;
		for (auto& e : fs::directory_iterator(dir, ec))
		{
			if (e.path() == keep) continue;
			std::error_code rc;
			fs::remove(e.path(), rc);
		}
	}

	std::filesystem::file_time_type HotReloadManager::newestSourceTime(bool cmakeOnly) const
	{
		fs::file_time_type newest = fs::file_time_type::min();
		for (const auto& root : m_cfg.sourceDirs)
		{
			// ディレクトリの存在確認
			std::error_code ec;
			if (!fs::exists(root, ec)) continue;
			// ディレクトリの探索
			for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
			{
				if (ec) break;
				// 読み込めない/読み込みたくないファイルをはじく
				if (!it->is_regular_file(ec) || !isSourceFile(it->path()) || 
					(cmakeOnly && it->path().filename() != "CMakeLists.txt")) continue;
				// 最終更新日を取得して更新されていたら置き換え
				auto t = fs::last_write_time(it->path(), ec);
				if (!ec && t > newest) newest = t;
			}
		}
		return newest;
	}

	bool HotReloadManager::syncShaders()
	{
		std::error_code ec;
		if (m_cfg.shaderRuntimeDir.empty() || !fs::exists(m_cfg.shaderRuntimeDir, ec)) return false;

		auto isShader = [](const fs::path& p) {auto e = p.extension().string(); return e == ".hlsl" || e == ".hlsli"; };

		if (!m_cfg.shaderSourceDir.empty() && fs::exists(m_cfg.shaderSourceDir, ec))
		{
			for (fs::recursive_directory_iterator it(m_cfg.shaderSourceDir, ec), end; it != end; it.increment(ec))
			{
				if (ec) break;
				if(!it->is_regular_file(ec) || !isShader(it->path())) continue;
				const auto rel = fs::relative(it->path(), m_cfg.shaderSourceDir, ec);
				const auto dst = m_cfg.shaderRuntimeDir / rel;
				if (!fs::exists(dst, ec) || fs::last_write_time(it->path(), ec) > fs::last_write_time(dst, ec))
				{
					fs::create_directories(dst.parent_path(), ec);
					fs::copy_file(it->path(), dst, fs::copy_options::overwrite_existing, ec);
				}
			}
		}

		fs::file_time_type newest = fs::file_time_type::min();
		for (fs::recursive_directory_iterator it(m_cfg.shaderRuntimeDir, ec), end; it != end; it.increment(ec))
		{
			if (ec) break;
			if (!it->is_regular_file(ec) || !isShader(it->path())) continue;
			auto t = fs::last_write_time(it->path(), ec);
			if (!ec && t > newest) newest = t;
		}

		const bool changed = newest > m_shaderStamp;
		m_shaderStamp = newest;

		return changed;
	}
}
