#pragma onces
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <glm/glm.hpp>

namespace FaluEngine
{
	enum class BodyType {
		Static,
		Dynamic,
		Kinematic,
	};

	enum class ColliderShape {
		Box,
		Sphere,
		Capsule,
	};

	struct RigidbodyComponent {
		BodyType bodyType = BodyType::Dynamic;
		ColliderShape shape = ColliderShape::Box;

		glm::vec3 halfExtents = { 0.5f,0.5f,0.5f };
		float radius = 0.5f;
		float height = 1.0f;

		float mass = 1.0f;
		float restitution = 0.3f;
		float friction = 0.5f;
		bool useGravity = true;

		JPH::BodyID bodyID = JPH::BodyID();
		bool registered = false;
	};
}

