#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include "PhysicsLayer.h"
#include <memory>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace FaluEngine {

	class Scene;

	struct RaycastHit
	{
		bool hit = false;
		entt::entity entity = entt::null;
		glm::vec3 point{};
		glm::vec3 normal{};
		float distance = 0.0f;
	};

	class PhysicsSystem
	{
	public:
		static PhysicsSystem& get() {
			static PhysicsSystem instance;
			return instance;
		}


		bool init(
			uint32_t maxBodies = 1024,
			uint32_t maxBodyPairs = 1024,
			uint32_t maxContactConstraints = 1024);

		void shutdown();

		void registerScene(Scene& scene);

		void unregisterScene(Scene& scene);

		void step(float deltaTime, int subSteps = 1);

		void syncTransforms(Scene& scene);

		[[nodiscard]] JPH::PhysicsSystem* getWorld() noexcept { return m_physicsSystem.get(); }
		[[nodiscard]] JPH::BodyInterface& getBodyInterface() noexcept {
			return m_physicsSystem->GetBodyInterface();
		}
		[[nodiscard]] RaycastHit raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance);

	private:
		PhysicsSystem() = default;
		~PhysicsSystem() { shutdown(); }
	private:
		bool m_initialized = false;

		std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
		std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
		std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;

		BPlayerInterfaceImpl m_bpLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl m_objVsBpFilter;
		ObjectLayerPairFilterImpl m_objLayerFiler;
	};
}
