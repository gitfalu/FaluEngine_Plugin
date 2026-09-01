#pragma once
#include "script/NativeScript.h"
#include "scene/Entity.h"


class PlayerController : public FaluEngine::NativeScript
{
public:
	void onInit(FaluEngine::Entity& entity) override
	{

	}

	void onUpdate(FaluEngine::Entity& entity, float deltaTime) override;

	void onDestroy(FaluEngine::Entity& entity) override {}
	void onClick(FaluEngine::Entity& entity) override {}

};
