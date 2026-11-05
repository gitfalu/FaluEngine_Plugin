/*****************************************************************//**
 * \file   SpriteTypes.h
 * \brief  スプライトのデータを定義
 * 
 * \author tsunn
 * \date   November 2026
 *********************************************************************/
#include <string>
#include <glm/glm.hpp>
#include <asset/AssetManager.h>
#include <FaluEngine/ActionEventTrack.h>

namespace FaluEngine
{

/// @brief スプライト一つの情報
struct SpriteSlice
{
	std::string name;
	glm::vec4 rectPx;
	glm::vec2 pivot{ 0.5f,0.5f };
};

/// @brief 
struct SpriteSheetAsset : Asset
{
	std::string texturePath;
	glm::ivec2 texSize{};
	std::vector<SpriteSlice> slices;
};

/// @brief 現在のフレーム情報(番号、継続時間)
struct SpriteFrame
{
	int slice = 0;
	float duration = 1.0f / 12.0f;
};

/// @brief スプライトアニメーションのRuntime管理情報
struct SpriteClipAsset : Asset
{
	std::string sheetPath;
	std::vector<SpriteFrame> frames;
	bool loop = true;
	std::vector<ActionPointEvent> events;
	float totalDuration() const;
	int frameAt(float t) const;
};

}