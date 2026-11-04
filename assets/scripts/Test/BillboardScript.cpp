#include "BillboardScript.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/Application.h"
#include <glm/gtc/matrix_transform.hpp>

void BillboardScript::onUpdate(FaluEngine::Entity& entity, float deltaTime)
{
	auto& t = entity.getComponent<FaluEngine::TransformComponent>();
	glm::vec3 camPos = FaluEngine::Application::getInstance().getCameraPosition();

	glm::vec3 dir = camPos - t.position;
	dir.y = 0.0f;
	if (glm::length(dir) > 0.0001f)
	{
		float yaw = atan2(dir.x, dir.z);
		t.setRotationEuler({ 0.0f,glm::degrees(yaw),0.0f });
	}

}

REGISTER_NATIVE_SCRIPT(BillboardScript)

