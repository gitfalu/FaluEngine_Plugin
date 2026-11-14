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

	//==== エディタ編集用(逆変換・アンカープリセット・リサイズ・スナップ) =====
	/// @brief 親の矩形。Canvas直下なら中心(0,0)・Canvasサイズ・回転 0
	struct ParentFrame
	{
		glm::vec2 pos = { 0.0f,0.0f };
		glm::vec2 size = { 0.0f,0.0f };
		float rotation = 0.0f;
	};

	/// @brief 
	/// @param registry 
	/// @param entity 
	/// @return 
	FALU_SCENE_API ParentFrame parentFrameOf(entt::registry& registry, entt::entity entity);

	/// @brief 
	/// @param registry 
	/// @param entity 
	/// @return 
	FALU_SCENE_API void relayout(entt::registry& registry, entt::entity entity);

	/// @brief 
	/// @param registry 
	/// @param entity 
	/// @param center 
	/// @param scaledSize 
	/// @param totalRotationDeg 
	/// @return 
	FALU_SCENE_API void setWorldRect(entt::registry& registry, entt::entity entity,
		const glm::vec2& center, const glm::vec2& scaledSize,
		float totalRotationDeg);

	//======  =======
	enum class HAlign { Left, Center, Right, Stretch };
	enum class VAlign { Top, Middle, Bottom, Stretch };

	/// @brief 
	/// @param registry 
	/// @param entity 
	/// @param h 
	/// @param v 
	/// @return 
	FALU_SCENE_API void applyAnchorPress(entt::registry& registry, entt::entity entity, HAlign h, VAlign v);

	/// @brief 回転を持つ矩形
	struct RectGeom
	{
		glm::vec2 center = { 0.0f,0.0f };
		glm::vec2 size = { 0.0f,0.0f };
		float rotation = 0.0f; // Degree
	};

	/// @brief ハンドルの向き。sx/syは-1(左/下)・0・+1(右/上)。レイアウト空間はY上向き
	struct HandleDir { int sx = 0; int sy = 0; };

	/// @brief 
	/// @param orig 
	/// @param h 
	/// @param deltaWorld 
	/// @return 
	FALU_SCENE_API RectGeom resizeRect(const RectGeom& orig, HandleDir h, const glm::vec2& deltaWorld,
		bool symmetric,bool keepAspect,float minSize = 1.0f);

	struct SnapResult
	{
		glm::vec2 offset = { 0.0f,0.0f }; // 矩形に足すと吸着する量
		bool snappedX = false, snappedY = false;
		float guideX = 0.0f, guideY = 0.0f; // 吸着した目標座標
	};

	/// @brief 軸平行な矩形(左下 rectMin・size)を、親の端/中心=>グリッドの純で吸着させるオフセットを求める
	/// @param rectMin 
	/// @param size 
	/// @param parentMin 
	/// @param parentSize 
	/// @param gridSize 
	/// @param threshold 
	/// @return 
	FALU_SCENE_API SnapResult computeSnapOffset(const glm::vec2& rectMin, const glm::vec2& size,
		const glm::vec2& parentMin, const glm::vec2& parentSize,
		float gridSize, float threshold);
}