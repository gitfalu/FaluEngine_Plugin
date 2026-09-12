#pragma once
#include "FaluEngine/NativeScript.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"

class GameManager : public FaluEngine::NativeScript
{
public:
	static GameManager& get() { return *s_instance; }

	void onInit(FaluEngine::Entity& entity) override;
	void onUpdate(FaluEngine::Entity& entity, float deltaTime) override;

	void onDestroy(FaluEngine::Entity& entity) override {}
	void onClick(FaluEngine::Entity& entity) override {}

	void addScore(float v) { m_score += v; }
	bool hasPlayer() { return m_player.isValid(); }
	FaluEngine::TransformComponent& getPlayerTransform();
	void spawnHitEffect(FaluEngine::Scene* scene,const glm::vec3& pos);

private:
	void spawnCollectibles(FaluEngine::Entity& self, int count);
	void drawHud();


private:
	static GameManager* s_instance;
	float m_score = 0.0f;
	float m_timeLimit = 60.0f;
	float m_timeLeft = 60.0f;
	bool m_finished = false;

	FaluEngine::Entity m_player;
	FaluEngine::Entity m_fadeEntity;

};
