#pragma once

#include <filesystem>
#include <string>
#include "Logger.h"

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #endif // WIN32_LEAN_AND_MEAN
 #include <Windows.h>

#endif // _WIN32


namespace FaluEngine {
	class PathResolver {
	public:
		static void Init(const std::filesystem::path& exePath = getExePath()) {
			if (std::filesystem::exists(std::filesystem::current_path() / "assets")) {
				s_root = std::filesystem::current_path();
				LOG_INFO("PathResolver: root = '{}' (cwd)", s_root.string());
				return;
			}

			const std::vector<std::string> candidates = {
				"bin/Debug","bin/Release",
				"bin/Debug/","bin/Release/",
			};

			auto cwd = std::filesystem::current_path();
			for (const auto& c : candidates)
			{
				auto candidate = cwd / c;
				if (std::filesystem::exists(candidate / "assets")) {
					s_root = candidate;
					LOG_INFO("PathResolver: root = '{}' (subdirectory", s_root.string());
					return;
				}
			}

			auto exeDir = exePath.parent_path();
			if (std::filesystem::exists(exeDir / "assets")) {
				s_root = exeDir;
				LOG_INFO("PathResolver: root = '{}' (exe dir)", s_root.string());
				return;
			}

			s_root = std::filesystem::current_path();
			LOG_WARN("PathREsolver: assets/ not found, using ced = '{}'", s_root.string());
		}

		[[nodiscard]] static std::filesystem::path resolve(const std::string& relativePath) {
			return s_root / relativePath;
		}

		[[nodiscard]] static std::string resolveStr(const std::string& relativePath) {
			return resolve(relativePath).string();
		}

		[[nodiscard]] static const std::filesystem::path& getRoot() noexcept {
			return s_root;
		}

	private:
		static std::filesystem::path getExePath() {
#ifdef _WIN32
			wchar_t buf[MAX_PATH] = {};
			GetModuleFileNameW(nullptr, buf, MAX_PATH);
			return std::filesystem::path(buf);
#else
			return std::filesystem::canonical("/proc/self/exe");
#endif // _WIN32

		}

		static inline std::filesystem::path s_root;
	};
}
