/*****************************************************************//**
 * \file   SimulationSystem.h
 * \brief  メッシュ変形シミュレーションの更新の入口を作成するスクリプト
 * 
 * \author tsunn
 * \date   November 2026
 *********************************************************************/
#pragma once

#include "RippleComponent.h"
#include "ClothComponent.h"
#include "WindComponent.h"
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class Scene;
	class Entity;

	/// @brief シミュレーションの更新
	/// @param scene アクティブシーンの参照
	/// @param dt デルタタイム
	/// @return 
	FALU_SCENE_API void updateSimulation(Scene& scene, float dt);

	/// @brief コンポーネントを付与する際にMeshが無ければ一緒に付与する
	/// @param scene 現在のシーン
	/// @param entity 付与するエンティティ
	/// @param params 付与する際の情報
	/// @return 
	FALU_SCENE_API void addRippleComponents(Scene& scene, Entity entity, RippleComponent params = {});

	/// @brief ワールド座標基準で波紋を発生させる。スクリプトなどから呼ぶことができる
	/// @param scene アクティブシーンの参照
	/// @param entity 選択されているエンティティ
	/// @param worldPos ワールド座標
	/// @param strength 強さ
	/// @param radius 半径(デフォルトで1.0)
	/// @return 
	FALU_SCENE_API void addRippleImpulse(Scene& scene, entt::entity entity,
		const glm::vec3& worldPos, float strength = 0.4f, float radius = 1.0f);

	/// @brief 状態をリセットさせる(水面を静止させる)
	/// @param ripple リセットする水面
	/// @return 
	FALU_SCENE_API void resetRipple(RippleComponent& ripple);

	/// @brief コンポーネントを付与する際にMeshが無ければ一緒に付与する
	/// @param scene 現在のシーン
	/// @param entity 付与対象のエンティティ
	/// @param params 付与する際の情報
	/// @return 
	FALU_SCENE_API void addClothComponents(Scene& scene, Entity entity, ClothComponent params = {});

	/// @brief 布の状態をリセットする
	/// @param scene 
	/// @param entity 
	/// @return 
	FALU_SCENE_API void resetCloth(Scene& scene, entt::entity entity);

	/// @brief 現在のアクティブシーンで吹いている風を取得する
	/// @return 
	FALU_SCENE_API const WindParams& getSceneWind();

	/// @brief 現在の位置に発生している風のベクトルを取得
	/// @param world 
	/// @return 
	FALU_SCENE_API glm::vec3 sampleSceneWind(const glm::vec3& world);
}
