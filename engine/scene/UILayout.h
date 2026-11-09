#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <FaluEngine/EngineExport.h>
#include "FaluEngine/UITypes.h"

/// @brief UIのレイアウト計算・描画順・当たり判定。シーンはここを呼ぶだけ、レンダラーやウィンドウには直接触れない
namespace FaluEngine::UILayout
{
	struct CanvasMetrics
	{
		glm::vec2 size = { 0.0f,0.0f }; // レイアウト空間(仮想ピクセル)のサイズ
		float scale = 1.0f; // 仮想ピクセル->実ピクセルの倍率
	};

	/// @brief Canvasのスケール設定と実画面サイズから、レイアウト空間をスケールを求める
	/// @param canvas 
	/// @param screenSize 
	/// @return 
	FALU_SCENE_API CanvasMetrics computeCanvasMetrics(const CanvasComponent& canvas,
		const glm::vec2& screenSize);

	/// @brief 1つのRectTransformを親の矩形から計算し子へ再帰する。回転はPivotを中心に回り親の回転はこの位置と向きに伝播する
	/// @param registry 
	/// @param entity 
	/// @param parentPos 
	/// @param parentSize 
	/// @param parentRotationDeg 
	/// @return 
	FALU_SCENE_API void computeRect(entt::registry& registry, entt::entity entity,
		const glm::vec2& parentPos, const glm::vec2& parentSize,
		float parentRotationDeg = 0.0f);

	/// @brief Canvasのランタイム値を更新し、子をすべてレイアウトする
	/// @param registry 
	/// @param canvasEntity 
	/// @param screenSize 
	/// @return 
	FALU_SCENE_API void layoutCanvas(entt::registry& registry, entt::entity canvasEntity,
		const glm::vec2& screenSize);

	/// @brief 有効なScreenSpaceOverlayのCanvasをsortOrder　昇順で返す
	/// @param registry 
	/// @return 
	FALU_SCENE_API std::vector<entt::entity> collectCanvases(entt::registry& registry);

	/// @brief Canvas配下を深さ優先で並べる。後ろほど手前に描かれる、RectTransformを持たなければ対象外になる
	/// @param registry 
	/// @param canvasEntity 
	/// @param out 
	/// @return 
	FALU_SCENE_API void collectDrawOrder(entt::registry& registry, entt::entity canvasEntity,
		std::vector <entt::entity>& out);

	/// @brief レイアウト空間の点pが、回転を考慮して矩形内にあるか
	/// @param rt 
	/// @param p 
	/// @return 
	FALU_SCENE_API bool hitTest(const RectTransformComponent& rt, const glm::vec2& p);
}