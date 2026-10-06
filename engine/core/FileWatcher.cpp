#include "FileWatcher.h"
#include "core/PathResolver.h"

void FileWatcher::watch(const std::string& path, std::function<void()> onChanged)
{
	auto fullPath = FaluEngine::PathResolver::resolve(path);

	std::error_code ec;
	const auto ts = std::filesystem::last_write_time(fullPath, ec);
	if (ec) return;
	
	m_watched[path] = { path,fullPath,ts,std::move(onChanged) };
}

void FileWatcher::poll()
{
	const auto now = std::chrono::steady_clock::now();
	if (now - m_lastPoll < kPollInterval) return;
	m_lastPoll = now;

	for (auto& [assetPath, entry] : m_watched)
	{
		std::error_code ec;
		auto ts = std::filesystem::last_write_time(entry.fullpath,ec);
		if (ec) continue;

		if (ts != entry.lastWriteTime)
		{
			entry.lastWriteTime = ts;
			entry.onChanged();
		}
	}
}
