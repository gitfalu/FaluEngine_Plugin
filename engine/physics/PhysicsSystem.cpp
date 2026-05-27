#include "PhysicsSystem.h"
#include "RigidbodyComponent.h"
#include "core/Logger.h"
#include "scene/Scene.h"
#include "scene/Component.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <glm/glm.hpp>


namespace FaluEngine
{
	static JPH::Vec3 toJPH(const glm::vec3& v) { return { v.x,v.y,v.z }; }
	static JPH::Quat toJPH(const glm::quat& q) { return { q.x,q.y,q.z,q.w }; }
	static glm::vec3 toGLM(const JPH::Vec3& v) { return { v.GetX(),v.GetY(),v.GetZ()}; }
	static glm::quat toGLM(const JPH::Quat& q) { return { q.GetW(),q.GetX(),q.GetY(),q.GetZ() }; }

	

	bool PhysicsSystem::init(uint32_t maxBodies, uint32_t maxBodyPairs, uint32_t maxContactConstraints)
	{
		JPH::RegisterDefaultAllocator();
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

		m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
		m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
			JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 2
		);
		m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
		m_physicsSystem->Init(
			maxBodies, 0, maxBodyPairs, maxContactConstraints,
			m_bpLayerInterface, m_objVsBpFilter, m_objLayerFiler
		);

		m_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.8f, 0.0f));

		m_initialized = true;
		LOG_INFO("PhysicsSystem (Jolt) initialized");

		return true;
	}

	void PhysicsSystem::shutdown()
	{
		if (!m_initialized) return;

		m_physicsSystem.reset();
		m_jobSystem.reset();
		m_tempAllocator.reset();

		JPH::UnregisterTypes();
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;

		m_initialized = false;
		LOG_INFO("PhysicsSystem shutdown");
	}

	void PhysicsSystem::registerScene(Scene& scene)
	{
		auto& bodyInterface = m_physicsSystem->GetBodyInterface();
		auto view = scene.registry().view<RigidbodyComponent, TransformComponent>();

		for (auto entity : view)
		{
			auto& rb = view.get<RigidbodyComponent>(entity);
			auto& transform = view.get<TransformComponent>(entity);

			if (rb.registered) continue;

			JPH::RefConst<JPH::Shape> shape;
			float minExtent, convexRadius;
			switch (rb.shape)
			{
			case ColliderShape::Box:
				minExtent = (std::min)({ rb.halfExtents.x,rb.halfExtents.y,rb.halfExtents.z });
				convexRadius = (std::min)(minExtent * 0.1f, 0.05f);
				shape = new JPH::BoxShape(toJPH(rb.halfExtents),convexRadius);
				break;
			case ColliderShape::Sphere:
				shape = new JPH::SphereShape(rb.radius);
				break;
			case ColliderShape::Capsule:
				shape = new JPH::CapsuleShape(rb.height * 0.5f, rb.radius);
				break;
			}

			JPH::EMotionType motionType;
			JPH::ObjectLayer layer;
			switch (rb.bodyType)
			{
			case BodyType::Static:
				motionType = JPH::EMotionType::Static;
				layer = Layers::MOVING;
				break;
			case BodyType::Kinematic:
				motionType = JPH::EMotionType::Kinematic;
				layer = Layers::MOVING;
				break;
			default:
				motionType = JPH::EMotionType::Dynamic;
				layer = Layers::MOVING;
				break;
			}

			JPH::BodyCreationSettings settings(
				shape,
				toJPH(transform.position),
				toJPH(transform.rotation),
				motionType,
				layer
			);

			settings.mRestitution = rb.restitution;
			settings.mFriction = rb.friction;
			if (!rb.useGravity)
				settings.mGravityFactor = 0.0f;

			rb.bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
			rb.registered = true;

			LOG_TRACE("PhysicsSystem: registered body for entity");
		}

		m_physicsSystem->OptimizeBroadPhase();
	}

	void PhysicsSystem::unregisterScene(Scene& scene)
	{
		auto& bodyInterface = m_physicsSystem->GetBodyInterface();
		auto view = scene.registry().view<RigidbodyComponent>();

		for(auto entity : view)
		{ 
			auto& rb = view.get<RigidbodyComponent>(entity);
			if (!rb.registered) continue;

			bodyInterface.RemoveBody(rb.bodyID);
			bodyInterface.DestroyBody(rb.bodyID);
			rb.registered = false;
		}
	}

	void PhysicsSystem::step(float deltaTime, int subSteps)
	{
		if (!m_initialized) return;
		m_physicsSystem->Update(deltaTime, subSteps, m_tempAllocator.get(), m_jobSystem.get());
	}

	void PhysicsSystem::syncTransforms(Scene& scene)
	{
		auto& bodyInterfase = m_physicsSystem->GetBodyInterface();
		auto view = scene.registry().view<RigidbodyComponent, TransformComponent>();

		for (auto entity : view)
		{
			auto& rb = view.get<RigidbodyComponent>(entity);
			auto& transform = view.get<TransformComponent>(entity);

			if (!rb.registered || rb.bodyType == BodyType::Static) continue;

			transform.position = toGLM(bodyInterfase.GetPosition(rb.bodyID));
			transform.rotation = toGLM(bodyInterfase.GetRotation(rb.bodyID));
		}
	}

}
