#include "PlayerController.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "FaluEngine/InputManager.h"
#include "FaluEngine/Component.h"

void PlayerController::onUpdate(FaluEngine::Entity& entity, float deltaTime)
{
	auto& t = entity.getComponent<FaluEngine::TransformComponent>();
	auto& input = FaluEngine::InputManager::get();

	glm::mat4 matrix = t.getMatrix();

	glm::vec3 forward = glm::normalize(glm::vec3(matrix[2]));

	if (input.isKeyDown(FaluEngine::Key::W))
	{
		t.position += forward * deltaTime * 10.0f;
	}
	if (input.isKeyDown(FaluEngine::Key::S))
	{
		t.position -= forward * deltaTime * 10.0f;
	}


	glm::vec3 newEuler = t.rotationEulerHint;

	if (input.isKeyDown(FaluEngine::Key::Up))
	{
		newEuler.x += deltaTime * 10.0f;
	}

	if (input.isKeyDown(FaluEngine::Key::Down))
	{
		newEuler.x -= deltaTime * 10.0f;
	}

	if (input.isKeyDown(FaluEngine::Key::Left))
	{
		newEuler.y += deltaTime * 10.0f;
	}

	if (input.isKeyDown(FaluEngine::Key::Right))
	{
		newEuler.y -= deltaTime * 10.0f;
	}

	t.setRotationEuler(newEuler);
}

REGISTER_NATIVE_SCRIPT(PlayerController)

