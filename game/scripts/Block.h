#pragma once
#include "FaluEngine/NativeScript.h"
#include "FaluEngine/Entity.h"


class Block : public FaluEngine::NativeScript
{
public:
	void onInit(FaluEngine::Entity& entity) override;
	
	void onUpdate(FaluEngine::Entity& entity, float deltaTime) override;

	void onDestroy(FaluEngine::Entity& entity) override {}
	void onClick(FaluEngine::Entity& entity) override {}

	float value = 10.0f;
private:
	float m_time = 0.0f;
	float m_baseY = 0.0f;
	bool m_hit = false;
};
