#include "FileWatcher.h"

void FileWatcher::watch(const std::string& path, std::function<void()> onChanged)
{
	if (!std::filesystem::exists(path)) return;
	m_watched[path] = { std::filesystem::last_write_time(path),std::move(onChanged) };
}

void FileWatcher::poll()
{
	for (auto& [path, entry] : m_watched)
	{
		if (!std::filesystem::exists(path)) continue;
		auto ts = std::filesystem::last_write_time(path);
		if (ts != entry.lastWriteTime)
		{
			entry.lastWriteTime = ts;
			entry.onChanged();
		}
	}
}
