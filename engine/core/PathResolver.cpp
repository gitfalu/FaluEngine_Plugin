#include "PathResolver.h"

namespace FaluEngine
{
	void PathResolver::Init(const std::filesystem::path& exePath)
	{
		auto exeDir = exePath.parent_path();

		if (std::filesystem::exists(exeDir / "assets")) {
			s_root = exeDir;
			s_assetRoot = s_root / "assets";
			LOG_INFO("PathResolver: root = '{}' (cwd)", s_root.string());
			return;
		}

		const std::vector<std::filesystem::path> candidates = {
			exeDir / "bin/Debug", exeDir / "bin/Release",
			exeDir / "bin/Debug/", exeDir / "bin/Release/",
		};

		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate / "assets"))
			{
				s_root = std::filesystem::absolute(candidate).lexically_normal();
				s_assetRoot = s_root / "assets";
				LOG_INFO("PathResolver: root = '{}' (candidate dir)", s_root.string());
				return;
			}
		}

		auto cwd = std::filesystem::current_path();
		if (std::filesystem::exists(cwd / "assets"))
		{
			s_root = cwd;
			s_assetRoot = s_root / "assets";
			LOG_INFO("PathResolver: root = '{}' (cwd)", s_root.string());
			return;
		}

		s_root = cwd;
		s_assetRoot = s_root / "assets";
		LOG_WARN("PathREsolver: assets/ not found, using ced = '{}'", s_root.string());
	}

	std::filesystem::path PathResolver::resolve(const std::string_view path)
	{
		auto p = fromUtf8(path);

		if (p.is_absolute())
			return p.lexically_normal();

		return (s_root / p).lexically_normal();
	}

	std::filesystem::path PathResolver::fromUtf8(std::string_view utf8)
	{
#ifdef _WIN32
		return std::filesystem::u8path(
			utf8.begin(),
			utf8.end());
#else
		return std::filesystem::path(utf8);
#endif
	}

	std::string PathResolver::toUtf8(const std::filesystem::path& path)
	{
#ifdef _WIN32
		return fromWide(path.wstring());
#else
		return path.generic_string();
#endif
	}

	std::wstring PathResolver::toWide(const std::string& utf8)
	{
#ifdef _WIN32
		if (utf8.empty())
			return {};
		const int size =
			MultiByteToWideChar(
				CP_UTF8,
				MB_ERR_INVALID_CHARS,
				utf8.data(),
				static_cast<int>(utf8.size()),
				nullptr,
				0);
		if (size <= 0)
			return {};

		std::wstring result(size, L'\0');

		MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8.data(),
			static_cast<int>(utf8.size()),
			result.data(),
			size);

		return result;
#else
		return std::wstring(utf8.begin(), utf8.end());
#endif
	}

	std::string PathResolver::fromWide(std::wstring_view wide)
	{
#ifdef _WIN32
		if (wide.empty())
			return {};

		const int size =
			WideCharToMultiByte(
				CP_UTF8,
				WC_ERR_INVALID_CHARS,
				wide.data(),
				static_cast<int>(wide.size()),
				nullptr,
				0,
				nullptr,
				nullptr
			);

		if (size <= 0)
			return {};

		std::string result(size, '\0');

		WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			wide.data(),
			static_cast<int>(wide.size()),
			result.data(),
			size,
			nullptr,
			nullptr);

		return result;
#else
		return std::string(wide.begin(), wide.end());
#endif
	}

	std::string PathResolver::toAssetPath(const std::filesystem::path& absolutePath)
	{
		const auto normalized = absolutePath.lexically_normal();

		const auto relative = normalized.lexically_relative(s_root);

		if (relative.empty())
			return {};

		const auto generic = relative.generic_string();

		if (generic != "assets" && generic.rfind("assets/", 0) != 0)
		{
			return {};
		}

		return generic;
	}

	std::filesystem::path PathResolver::assetPathToFilesystem(std::string_view assetPath)
	{
		return resolve(assetPath);
	}

	std::string PathResolver::normalizeAssetPath(std::string_view path)
	{
		const auto fsPath = fromUtf8(path);

		if (fsPath.is_absolute())
			return toAssetPath(fsPath);

		return toAssetPath(resolve(path));
	}

	const std::filesystem::path& PathResolver::getRoot() noexcept
	{
		return s_root;
	}

	const std::filesystem::path& PathResolver::getAssetRoot() noexcept
	{
		return s_assetRoot;
	}

	std::filesystem::path PathResolver::getExePath()
	{
#ifdef _WIN32
		wchar_t buf[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		return std::filesystem::path(buf);
#else
		return std::filesystem::canonical("/proc/self/exe");
#endif // _WIN32

	}

}
