#pragma once
#include "FaluEngine/NativeScript.h"
#include "FaluEngine/Entity.h"


class HitEffectScript : public FaluEngine::NativeScript
{
public:
	void onInit(FaluEngine::Entity& entity) override
	{

	}

	void onUpdate(FaluEngine::Entity& entity, float deltaTime) override;

	void onDestroy(FaluEngine::Entity& entity) override {}
	void onClick(FaluEngine::Entity& entity) override {}

private:
	float m_time = 0.0f;
	static constexpr float kLifetime = 0.4f;
};
