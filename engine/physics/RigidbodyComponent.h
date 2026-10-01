#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <glm/glm.hpp>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	enum class FALU_ENGINE_API BodyType {
		Static,
		Dynamic,
		Kinematic,
	};

	enum class FALU_ENGINE_API ColliderShape {
		Box,
		Sphere,
		Capsule,
		HeightField,
	};

	struct FALU_ENGINE_API RigidbodyComponent {
		BodyType bodyType = BodyType::Dynamic;
		ColliderShape shape = ColliderShape::Box;

		glm::vec3 halfExtents = { 0.5f,0.5f,0.5f };
		float radius = 0.5f;
		float height = 1.0f;

		float mass = 1.0f;
		float restitution = 0.3f;
		float friction = 0.5f;
		bool useGravity = true;

		const float* heightFieldData = nullptr;

		uint32_t heightFieldSamples = 0;

		glm::vec3 heightFieldScale = { 1.0f,1.0f,1.0f };

		glm::vec3 heightFieldOffset = { 0.0f,0.0f,0.0f };

		JPH::BodyID bodyID = JPH::BodyID();
		bool registered = false;
	};
}

