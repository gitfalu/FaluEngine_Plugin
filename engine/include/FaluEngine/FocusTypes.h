#pragma once

#include <FaluEngine/EngineExport.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace FaluEngine
{
	struct FocusCategory
	{
		std::string name;
		glm::vec3 color = { 1.0f,1.0f,1.0f };
		int priority = 0;
	};

	struct FocusSettings
	{
		std::vector<FocusCategory> categories;

		[[nodiscard]] int findId(const std::string& name) const
		{
			for (size_t i = 0; i < categories.size(); ++i)
			{
				if (categories[i].name == name) return static_cast<int>(i) + 1;
			}
			return 0;
		}

		[[nodiscard]] static FocusSettings makeDefault()
		{
			FocusSettings s;
			s.categories = {
				{ "Untagged",      { 0.60f, 0.60f, 0.60f }, 0 },
				{ "Player",        { 0.20f, 0.60f, 1.00f }, 3 },
				{ "Enemy",         { 1.00f, 0.25f, 0.25f }, 4 },
				{ "Collectible",   { 1.00f, 0.85f, 0.10f }, 3 },
				{ "Terrain",       { 0.30f, 0.65f, 0.30f }, 0 },
				{ "Prop",          { 0.65f, 0.45f, 0.25f }, 0 },
				{ "UI-Critical",   { 1.00f, 0.40f, 0.80f }, 5 },
				{ "UI-Info",       { 0.30f, 0.90f, 0.90f }, 2 },
				{ "UI-Decoration", { 0.55f, 0.35f, 0.85f }, 0 },
			};
			return s;
		}
	};

	struct FocusCategoryStat
	{
		int id = 0;
		uint32_t pixels = 0;
		float ratio = 0.0f;
	};

	struct FocusEntityStat
	{
		entt::entity entity = entt::null;
		std::string name;
		int categoryId = 0;
		uint32_t pixels = 0;
		float ratio = 0.0f;
	};

	struct FocusMapResult
	{
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<uint8_t> category;
		std::vector<uint16_t> entityIndex;
		std::vector<entt::entity> entities;

		std::vector<FocusCategoryStat> categoryStats; // カテゴリごとのデータ
		std::vector<FocusEntityStat> entityStats; // ピクセルの多い順にソートするリスト
	};
}
