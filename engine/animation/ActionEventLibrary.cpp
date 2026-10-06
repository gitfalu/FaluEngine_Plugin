#include <FaluEngine/ActionEventLibrary.h>

#include "core/Logger.h"
#include "core/PathResolver.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace FaluEngine
{
	ActionEventLibrary& ActionEventLibrary::get()
	{
		static ActionEventLibrary instance;
		return instance;
	}

	std::string ActionEventLibrary::sidecarAssetPath(const std::string& meshPath)
	{
		return meshPath + ".actionevents.json";
	}

	ActionEventLibrary::FileEntry& ActionEventLibrary::ensureLoaded(const std::string& meshPath)
	{
		auto it = m_files.find(meshPath);
		if (it != m_files.end()) return it->second;

		FileEntry entry;
		const auto fsPath = PathResolver::resolve(sidecarAssetPath(meshPath));

		std::error_code ec;
		if (std::filesystem::exists(fsPath, ec))
		{
			std::ifstream ifs(fsPath, std::ios::binary);
			std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

			std::vector<ActionEventTrack> tracks;
			std::string err;
			if (actionTracksFromJson(text, tracks, err))
			{
				for (auto& t : tracks)
				{
					auto name = t.clipName;
					entry.tracks[name] = std::make_shared<ActionEventTrack>(std::move(t));
				}
			}
			else
			{
				// 壊れたファイルの場合読み込まず、上書きで消えないように警告を出す
				FALU_ENGINE_LOG_ERROR("ActionEventLibrary:failed to parse '{}': {}", fsPath.string(), err);
			}
		}
		++m_revision;
		return m_files.emplace(meshPath,std::move(entry)).first->second;
	}

	std::shared_ptr<const ActionEventTrack> ActionEventLibrary::find(const std::string& meshPath, const std::string& clipName)
	{
		auto& f = ensureLoaded(meshPath);
		auto it = f.tracks.find(clipName);
		return it != f.tracks.end() ? it->second : nullptr;
	}

	std::shared_ptr<ActionEventTrack> ActionEventLibrary::edit(const std::string& meshParh, const std::string& clipName)
	{
		auto& f = ensureLoaded(meshParh);
		auto& slot = f.tracks[clipName];
		if (!slot)
		{
			slot = std::make_shared<ActionEventTrack>();
			slot->clipName = clipName;
			++m_revision;
		}
		return slot;
	}

	void ActionEventLibrary::markEdited(const std::string& meshPath)
	{
		ensureLoaded(meshPath).dirty = true;
		++m_revision;
	}

	bool ActionEventLibrary::isDirty(const std::string& meshPath) const
	{
		auto it = m_files.find(meshPath);
		return it != m_files.end() && it->second.dirty;
	}

	bool ActionEventLibrary::save(const std::string& meshPath, std::string* errorOut)
	{
		auto& f = ensureLoaded(meshPath);

		std::vector<ActionEventTrack> tracks;
		for (auto& [name, t] : f.tracks)
		{
			if (t->windows.empty() && t->points.empty()) continue;
			t->sort();
			tracks.push_back(*t);
		}
		std::sort(tracks.begin(), tracks.end(),
			[](const ActionEventTrack& a, const ActionEventTrack& b) {return a.clipName < b.clipName; });

		const auto fsPath = PathResolver::resolve(sidecarAssetPath(meshPath));
		const auto tmpPath = std::filesystem::path(fsPath).concat(".tmp");

		auto fail = [&](const std::string& msg)
			{
				FALU_ENGINE_LOG_ERROR("ActionEventLibrary: save failed '{}': {}", fsPath.string(), msg);

				if (errorOut) *errorOut = msg;
				return false;
			};

		{
			std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
			if (!ofs) return fail("cannot open temp file");
			const std::string text = actionTracksToJson(tracks);
			ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
			ofs.flush();
			if (!ofs) return fail("write error");
		}

		std::error_code ec;
		std::filesystem::rename(tmpPath, fsPath, ec);
		if (ec)
		{
			std::filesystem::remove(tmpPath, ec);
			return fail("rename failed");
		}
		f.dirty = false;
		return true;
	}

	void ActionEventLibrary::reload(const std::string& meshPath)
	{
		m_files.erase(meshPath);
		++m_revision;
		(void)ensureLoaded(meshPath);
	}
}
