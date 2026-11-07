#pragma once
#include "asset/AssetManager.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <FaluEngine/EngineExport.h>

// スプライトシート(.fsprite)とスプライトクリップ(.fanim2d)s
// D3Dに依存しない純粋なデータ・テクスチャ本体はtexturePath経由でTextureAssetを使う
namespace FaluEngine
{
	struct SpriteSlice
	{
		std::string name;
		glm::vec4 rect = { 0.0f,0.0f,0.0f,0.0f };
		glm::vec2 pivot = { 0.5f,0.5f };
	};

	struct FALU_GRAPHICS_API SpriteSheetAsset : Asset
	{
		std::string texturePath;
		glm::ivec2 textureSize = { 0,0 };
		std::vector<SpriteSlice> slices;

		/// @brief スライスのUV矩形。範囲外やサイズ未設定なら全面
		/// @param sliceIndex 
		/// @return 
		[[nodiscard]] glm::vec4 uvRectOf(int sliceIndex) const;
	};

	struct SpriteFrame
	{
		int slice = 0;
		float duration = 1.0f / 12.0f; // 秒
	};

	/// @brief クリップ内のイベント(秒)
	struct SpriteClipEvent
	{
		float time = 0.0f;
		std::string name;
		std::string payload;
	};

	struct FALU_GRAPHICS_API SpriteClipAsset : Asset
	{
		std::string sheetPath;
		bool loop = true;
		std::vector<SpriteFrame> frames;
		std::vector<SpriteClipEvent> events;

		[[nodiscard]] float totalDuration() const;
	};

	/// @brief 時刻->フレームの純関数サンプル結果
	struct SpriteFrameSample
	{
		int frame = 0; // framesのインデックス
		int slice = 0; // シートのスライス番号
		bool finished = false; // 非ループで末尾に到達
		float wrappedTime = 0.0f; // ループ/クランプ後の時刻
	};

	/// @brief timeから再生位置を求める(状態を持たないので、巻き戻し/リプレイと整合する)
	/// @param clip 
	/// @param time 
	/// @return 
	FALU_GRAPHICS_API SpriteFrameSample sampleSpriteClip(const SpriteClipAsset& clip, float time);

	//====== 読み書き(JSON) ========
	FALU_GRAPHICS_API std::shared_ptr<SpriteSheetAsset> loadSpriteSheet(const std::string& path);
	FALU_GRAPHICS_API std::shared_ptr<SpriteClipAsset> loadSpriteClip(const std::string& path);
	FALU_GRAPHICS_API bool saveSpriteSheet(const SpriteSheetAsset& sheet, const std::string& path);
	FALU_GRAPHICS_API bool saveSpriteClip(const SpriteClipAsset& clip, const std::string& path);

	/// @brief .fsprite / .fanim2dのローダーをAssetManagerに登録する
	/// @return 
	FALU_GRAPHICS_API void registerSpriteLoaders();
}
