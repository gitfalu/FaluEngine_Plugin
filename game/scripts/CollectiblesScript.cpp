#include "CollectiblesScript.h"
#include "GameManager.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "FaluEngine/Application.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Audio.h"

void CollectiblesScript::onInit(FaluEngine::Entity& entity)
{
	m_baseY = entity.getComponent<FaluEngine::TransformComponent>().position.y;
}

void CollectiblesScript::onUpdate(FaluEngine::Entity& entity, float deltaTime)
{
	if (m_collected) return;
	m_time += deltaTime;

	auto& t = entity.getComponent<FaluEngine::TransformComponent>();

	t.position.y = m_baseY + sinf(m_time * 2.0f) * 0.2f;

	glm::vec3 e = t.rotationEulerHint;
	e.y += deltaTime * 60.0f;
	t.setRotationEuler(e);

	if (!GameManager::get().hasPlayer()) return;
	auto& player = GameManager::get().getPlayerTransform();


	float dist = glm::length(player.position - t.position);
	if (dist < 1.0f)
	{
		m_collected = true;
		GameManager::get().addScore(value);
		FaluEngine::playSound("assets/audio/collect.wav", 0.6f, true);
		GameManager::get().spawnHitEffect(entity.getScene(),t.position);
		entity.getScene()->destroyEntity(entity);
	}
}

REGISTER_NATIVE_SCRIPT(CollectiblesScript)

