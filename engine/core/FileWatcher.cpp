#include "FileWatcher.h"
#include "core/PathResolver.h"

void FileWatcher::watch(const std::string& path, std::function<void()> onChanged)
{
	auto fullPath = FaluEngine::PathResolver::resolve(path);
	if (!std::filesystem::exists(fullPath)) return;
	m_watched[path] = { path,std::filesystem::last_write_time(fullPath),std::move(onChanged) };
}

void FileWatcher::poll()
{
	for (auto& [assetPath, entry] : m_watched)
	{
		const auto fullPath = FaluEngine::PathResolver::resolve(assetPath);

		if (!std::filesystem::exists(fullPath)) continue;
		auto ts = std::filesystem::last_write_time(fullPath);

		if (ts != entry.lastWriteTime)
		{
			entry.lastWriteTime = ts;
			entry.onChanged();
		}
	}
}
