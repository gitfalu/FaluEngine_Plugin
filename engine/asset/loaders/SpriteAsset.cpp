#include "SpriteAsset.h"
#include "core/Logger.h"
#include "core/PathResolver.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace FaluEngine
{
	using json = nlohmann::json;

	//============= データ ===============
	glm::vec4 SpriteSheetAsset::uvRectOf(int sliceIndex) const
	{
		// サイズやインデックスが範囲外の場合初期UVを返す
		if (textureSize.x <= 0 || textureSize.y <= 0 ||
			sliceIndex < 0 || sliceIndex >= static_cast<int>(slices.size()))
			return { 0.0f,0.0f,1.0f,1.0f };

		// サイズからスプライトのUVを算出
		const glm::vec4& r = slices[sliceIndex].rect;
		const float w = static_cast<float>(textureSize.x);
		const float h = static_cast<float>(textureSize.y);
		return { r.x / w ,r.y / h,r.z / w, r.w / h };
	}

	float SpriteClipAsset::totalDuration() const
	{
		float total = 0.0f;
		for (const auto& f : frames) total += (std::max)(f.duration, 0.0f);
		return total;
	}

	SpriteFrameSample sampleSpriteClip(const SpriteClipAsset& clip, float time)
	{
		SpriteFrameSample out; 
		if (clip.frames.empty()) return out;

		const float total = clip.totalDuration();
		if (total <= 0.0f)
		{
			out.slice = clip.frames[0].slice;
			return out;
		}

		float t = (std::max)(time, 0.0f);
		if (clip.loop)
		{
			t = std::fmod(t, total);
		}
		else if (t >= total)
		{
			out.finished = true;
			out.frame = static_cast<int>(clip.frames.size()) - 1;
			out.slice = clip.frames.back().slice;
			out.wrappedTime = total;
			return out;
		}
		out.wrappedTime = t;

		float acc = 0.0f;
		for (size_t i = 0; i < clip.frames.size(); ++i)
		{
			acc += (std::max)(clip.frames[i].duration, 0.0f);
			if (t < acc)
			{
				out.frame = static_cast<int>(i);
				out.slice = clip.frames[i].slice;
				return out;
			}
		}
		out.frame = static_cast<int>(clip.frames.size()) - 1;
		out.slice = clip.frames.back().slice;
		return out;
	}

	//======= JSON ========
	namespace
	{
		bool readJsonFile(const std::string& path, json& out)
		{
			const auto fullPath = PathResolver::resolve(path);
			if (!std::filesystem::exists(fullPath))
			{
				LOG_ERROR("SpriteAsset: file not found '{}'", path);
				return false;
			}

			std::ifstream ifs(fullPath);
			if (!ifs)
			{
				LOG_ERROR("SpriteAsset: connot open '{}'", path);
				return false;
			}
			out = json::parse(ifs, nullptr,/*allow_exceptions=*/false);
			if (out.is_discarded() || !out.is_object())
			{
				LOG_ERROR("SpriteAsset: invalid json '{}'", path);
				return false;
			}
			return true;
		}

		bool writeJsonFile(const std::string& path, const json& j)
		{
			const auto fullPath = PathResolver::resolve(path);
			std::error_code ec;
			std::filesystem::create_directories(fullPath.parent_path(), ec);
			std::ofstream ofs(fullPath, std::ios::trunc);
			if (!ofs)
			{
				LOG_ERROR("SpriteAsset: connot write '{}'", path);
				return false;
			}
			ofs << j.dump(2);
			return static_cast<bool>(ofs);
		}
	}

	std::shared_ptr<SpriteSheetAsset> loadSpriteSheet(const std::string& path)
	{
		json j;
		if (!readJsonFile(path, j)) return nullptr;

		auto sheet = std::make_shared<SpriteSheetAsset>();
		sheet->texturePath = j.value("texture", "");
		if (j.contains("size") && j["size"].is_array() && j["size"].size() == 2)
			sheet->textureSize = { j["size"][0].get<int>(),j["size"][1].get<int>() };

		if (j.contains("slices") && j["slices"].is_array())
		{
			for (const auto& sj : j["slices"])
			{
				SpriteSlice s;
				s.name = sj.value("name", "");
				if (sj.contains("rect") && sj["rect"].is_array() && sj["rect"].size() == 4)
					s.rect = { sj["rect"][0].get<float>(), sj["rect"][1].get<float>(),
								sj["rect"][2].get<float>(), sj["rect"][3].get<float>() };
				if (sj.contains("pivot") && sj["pivot"].is_array() && sj["pivot"].size() == 2)
					s.pivot = { sj["pivot"][0].get<float>(),sj["pivot"][1].get<float>() };
				sheet->slices.push_back(std::move(s));
			}
		}
		return sheet;
	}

	std::shared_ptr<SpriteClipAsset> loadSpriteClip(const std::string& path)
	{
		json j;
		if (!readJsonFile(path, j)) return nullptr;

		auto clip = std::make_shared<SpriteClipAsset>();
		clip->sheetPath = j.value("sheet", "");
		clip->loop = j.value("loop", true);

		if (j.contains("frames") && j["frames"].is_array())
		{
			for (const auto& fj : j["frames"])
			{
				SpriteFrame f;
				f.slice = fj.value("slice", 0);
				f.duration = fj.value("duration", 1.0f / 12.0f);
				clip->frames.push_back(f);
			}
		}

		if (j.contains("events") && j["events"].is_array())
		{
			for (const auto& ej : j["events"])
			{
				SpriteClipEvent e;
				e.time = ej.value("time", 0.0f);
				e.name = ej.value("name", "");
				e.payload = ej.value("payload", "");
				clip->events.push_back(std::move(e));
			}
			std::stable_sort(clip->events.begin(), clip->events.end(),
				[](const SpriteClipEvent& a, const SpriteClipEvent& b) {return a.time < b.time; });
		}
		return clip;
	}

	bool saveSpriteSheet(const SpriteSheetAsset& sheet, const std::string& path)
	{
		json j;
		j["version"] = 1;
		j["texture"] = sheet.texturePath;
		j["size"] = { sheet.textureSize.x,sheet.textureSize.y };
		j["slices"] = json::array();
		for (const auto& s : sheet.slices)
		{
			j["slices"].push_back({
				{"name",s.name},
				{"rect",{s.rect.x,s.rect.y,s.rect.z,s.rect.w}},
				{"pivot",{s.pivot.x,s.pivot.y}}
				});
		}
		return writeJsonFile(path, j);
	}

	bool saveSpriteClip(const SpriteClipAsset& clip, const std::string& path)
	{
		json j;
		j["version"] = 1;
		j["sheet"] = clip.sheetPath;
		j["loop"] = clip.loop;
		j["frames"] = json::array();
		for (const auto& f : clip.frames)
			j["frames"].push_back({ { "slices",f.slice} , {"duration",f.duration} });
		j["events"] = json::array();
		for (const auto& e : clip.events)
			j["events"].push_back({ {"time",e.time} , {"name",e.name} ,{"payload",e.payload} });
		return writeJsonFile(path, j);
	}

	void registerSpriteLoaders()
	{
		auto& am = AssetManager::get();
		am.registerLoader<SpriteSheetAsset>([](const std::string& p) { return loadSpriteSheet(p); });
		am.registerLoader<SpriteClipAsset>([](const std::string& p) { return loadSpriteClip(p); });
	}

}
