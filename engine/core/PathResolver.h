#pragma once

#include <filesystem>
#include <string>
#include "Logger.h"

#include <FaluEngine/EngineExport.h>

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #endif // WIN32_LEAN_AND_MEAN
 #include <Windows.h>

#endif // _WIN32


namespace FaluEngine {
	class FALU_ENGINE_API PathResolver {
	public:
		static void Init(const std::filesystem::path& exePath = getExePath());

		/// @brief Engine root からの相対パス->絶対パス
		/// @param path 
		/// @return 
		[[nodiscard]] static std::filesystem::path resolve(const std::string_view path);

		/// @brief UTF-8 -> filesystem::path
		/// @param utf8 
		/// @return 
		[[nodiscard]] static std::filesystem::path fromUtf8(std::string_view utf8);

		/// @brief filesystem::path -> UTF-8
		/// @param path 
		/// @return 
		[[nodiscard]] static std::string toUtf8(const std::filesystem::path& path);
		
		/// @brief UTF-8 -> UTF-16
		/// @param wide 
		/// @return 
		[[nodiscard]] static std::wstring toWide(const std::string& utf8);

		/// @brief UTF-16 -> UTF-8
		/// @param relativePath 
		/// @return 
		[[nodiscard]] static std::string fromWide(std::wstring_view wide);

		/// @brief filesystem絶対path -> asset path
		/// @param absolutePath 
		/// @return 
		[[nodiscard]] static std::string toAssetPath(const std::filesystem::path& absolutePath);

		/// @brief AssetPath -> filesystem path
		/// @param assetPath 
		/// @return 
		[[nodiscard]] static std::filesystem::path assetPathToFilesystem(std::string_view assetPath);
		
		[[nodiscard]] static std::string normalizeAssetPath(std::string_view path);

		[[nodiscard]] static const std::filesystem::path& getRoot() noexcept;

		[[nodiscard]] static const std::filesystem::path& getAssetRoot() noexcept;

	private:
		static std::filesystem::path getExePath();

		static inline std::filesystem::path s_root;
		static inline std::filesystem::path s_assetRoot;
	};
}
