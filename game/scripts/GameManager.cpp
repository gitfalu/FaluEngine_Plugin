#include "GameManager.h"
#include "FaluEngine/Application.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "FaluEngine/InputManager.h"
#include "FaluEngine/Component.h"
#include "HitEffectScript.h"
#include "BillboardScript.h"
#include "CollectiblesScript.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Audio.h"
#include "FaluEngine/UI.h"

#include <imgui.h>

#include <cstdint>


GameManager* GameManager::s_instance = nullptr;

void GameManager::onInit(FaluEngine::Entity& entity)
{
	s_instance = this;
	m_timeLeft = m_timeLimit;

	FaluEngine::Entity player = entity.getScene()->findEntityByName("Player");
	if (player.isValid())
		m_player = player;

	spawnCollectibles(entity, 10);

	FaluEngine::playSound("assets/audio/bgm.wav", 0.6f, true);
}

void GameManager::onUpdate(FaluEngine::Entity& entity, float deltaTime)
{
	drawHud();

	if (m_finished) return;

	m_timeLeft -= deltaTime;
	if (m_timeLeft <= 0.0f)
	{
		m_finished = true;
		FaluEngine::playSound("assets/audio/timeup.wav", 0.6f, true);
		FaluEngine::Application::getInstance().switchScene("result");
	}
}

FaluEngine::TransformComponent& GameManager::getPlayerTransform()
{
	auto& trans = m_player.getComponent<FaluEngine::TransformComponent>();
	return trans;
}

void GameManager::spawnHitEffect(FaluEngine::Scene* scene,const glm::vec3& pos)
{
	if(!scene) return;
	auto e = scene->createEntity("hitEffect");
	e.getComponent<FaluEngine::TransformComponent>().position = pos;

	auto& nsc = e.getComponent<FaluEngine::NativeScriptComponent>();
	nsc.bindByName("HitEffectScript");
}

void GameManager::spawnCollectibles(FaluEngine::Entity& self, int count)
{
	auto* scene = self.getScene();
	for (int i = 0; i < count ;++i)
	{
		auto e = scene->createEntity("Collectible");
		auto& t = e.getComponent<FaluEngine::TransformComponent>();
		t.position = {
			(float)(rand() % 20 - 10),
			1.0f,
			(float)(rand() % 20 - 10)
		};

		// MeshŠ„‚è“–‚Ä
		// auto& mesh = e.addComponent<FaluEngine::MeshComponent>();
		// mesh.meshPath = "assets/meshes/coin.fbx";

		auto& nsc = e.addComponent<FaluEngine::NativeScriptComponent>();
		nsc.bindByName("CollectiblesScript");
	}
}

void GameManager::drawHud()
{
	char buf[64];
	snprintf(buf, sizeof(buf), "Score: %.0f", m_score);
	FaluEngine::drawText(20.0f, 20.0f, buf);

	snprintf(buf, sizeof(buf), "Time: %.1f", m_timeLeft > 0 ? m_timeLeft : 0.0f);
	FaluEngine::drawText(20.0f, 44.0f, buf);
}

REGISTER_NATIVE_SCRIPT(GameManager)

