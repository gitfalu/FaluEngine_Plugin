/*****************************************************************//**
 * \file   SimulationSystem.h
 * \brief  メッシュ変形シミュレーションの更新の入口を作成するスクリプト
 * 
 * \author tsunn
 * \date   November 2026
 *********************************************************************/
#pragma once

#include "RippleComponent.h"
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	class Scene;
	class Entity;

	/// @brief シミュレーションの更新
	/// @param scene 
	/// @param dt 
	/// @return 
	FALU_SCENE_API void updateSimulation(Scene& scene, float dt);

	FALU_SCENE_API void addRippleComponents(Scene& scene, Entity entity, RippleComponent params = {});

	/// @brief ワールド座標基準で波紋を発生させる。スクリプトなどから呼ぶことができる
	/// @param scene 
	/// @param entity 
	/// @param worldPos 
	/// @param strength 
	/// @param radius 
	/// @return 
	FALU_SCENE_API void addRippleImpulse(Scene& scene, entt::entity entity,
		const glm::vec3& worldPos, float strength = 0.4f, float radius = 1.0f);

	/// @brief 状態をリセットさせる(水面を静止させる)
	/// @param ripple 
	/// @return 
	FALU_SCENE_API void resetRipple(RippleComponent& ripple);
}
