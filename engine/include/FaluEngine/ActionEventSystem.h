/*****************************************************************//**
 * \file   ActionEventSystem.h
 * \brief  
 * 
 * \author tsunn
 * \date   October 2026
 *********************************************************************/
#pragma once
#include <FaluEngine/ActionEventTrack.h>
#include <FaluEngine/EngineExport.h>

#include <entt/entt.hpp>
#include <cstdint>
#include <string>

namespace FaluEngine
{
	struct AnimatorComponent;

	struct ActionWindowEvent
	{
		entt::entity entity = entt::null;
		uint64_t uuid = 0;
		ActionWindowType type = ActionWindowType::Parry;
		bool opened = true;
		float timeSeconds = 0.0f;
	};

	struct ActionPointFiredEvent
	{
		entt::entity entity = entt::null;
		uint64_t uuid = 0;
		std::string name;
		std::string payload;
		float timeSeconds = 0.0f;
	};

	class FALU_ENGINE_API ActionEventSystem
	{
	public:
		static void tick(entt::registry& reg, entt::entity e,
			const AnimatorComponent& animator, const std::string& meshPath,
			float clipDuration, float prevTime, bool wrapped, float deltaTime);

		static void resync(entt::registry& reg, entt::entity e, const AnimatorComponent& animator,
			const std::string& meshPath,float clipDuration);

		static void resyncAll(entt::registry& reg);

	private:
		static void publish(entt::registry& reg, entt::entity e, const ActionStateComponent& s, float timeSeconds);
	};
}
