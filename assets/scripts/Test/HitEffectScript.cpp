#include "HitEffectScript.h"
#include "FaluEngine/NativeScriptRegistry.h"
#include "FaluEngine/InputManager.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/Scene.h"

void HitEffectScript::onUpdate(FaluEngine::Entity& entity, float deltaTime)
{
	m_time += deltaTime;
	float t01 = m_time / kLifetime;

	auto& t = entity.getComponent<FaluEngine::TransformComponent>();
	t.scale = glm::vec3(1.0f + t01 * 2.0f);

	if (entity.hasComponent<FaluEngine::MeshComponent>())
	{
		// ”¼“§–¾ˆ—‚ðs‚¤
	}

	if (t01 >= 1.0f)
		entity.getScene()->destroyEntity(entity);

}

REGISTER_NATIVE_SCRIPT(HitEffectScript)

