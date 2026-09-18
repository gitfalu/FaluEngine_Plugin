#include "TitleButton.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/Application.h"
#include "FaluEngine/InputManager.h"
#include <glm/gtc/matrix_transform.hpp>

void TitleButton::onUpdate(FaluEngine::Entity& entity, float deltaTime)
{
	auto& input = FaluEngine::InputManager::get();
	if (input.isKeyPressed(FaluEngine::Key::Space))
	{
		FaluEngine::Application::getInstance().switchScene("game");
	}
}

void TitleButton::onClick(FaluEngine::Entity& entity)
{
	FaluEngine::Application::getInstance().switchScene("game");
}

REGISTER_NATIVE_SCRIPT(TitleButton)

