#pragma once
#include <functional>
#include <string>
#include <filesystem>
#include <unordered_map>

class FileWatcher
{
public:
	/// @brief 監視対象パスと、変更検知時のコールバックを登録
	/// @param path 
	/// @param onChanged 
	void watch(const std::string& path, std::function<void()> onChanged);

	/// @brief 毎フレーム更新
	void poll();

private:
	struct Entry 
	{
		std::filesystem::file_time_type lastWriteTime;
		std::function<void()> onChanged;
	};

	std::unordered_map<std::string, Entry> m_watched;
};
