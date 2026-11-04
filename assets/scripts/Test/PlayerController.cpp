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

	if (input.isKeyDown(FaluEngine::Key::A))
	{
		if(t.position.x < 5.0f)
			t.position.x += deltaTime * 10.0f;
	}
	if (input.isKeyDown(FaluEngine::Key::D))
	{
		if(t.position.x > -5.0f)
			t.position.x -= deltaTime * 10.0f;
	}
	if (input.isKeyPressed(FaluEngine::Key::Space))
	{
		t.position.x = 0.0f;
	}
	
}

REGISTER_NATIVE_SCRIPT(PlayerController)

