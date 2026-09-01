#include "PlayerController.h"
#include "script/NativeScriptRegistry.h"
#include "core/InputManager.h"
#include "scene/Component.h"

void PlayerController::onUpdate(FaluEngine::Entity& entity, float deltaTime)
{
	auto& t = entity.getComponent<FaluEngine::TransformComponent>();
	auto& input = FaluEngine::InputManager::get();

	if (input.isKeyDown(FaluEngine::Key::W))
	{
		t.position.z += deltaTime * 5.0f;
	}
}

REGISTER_NATIVE_SCRIPT(PlayerController)

