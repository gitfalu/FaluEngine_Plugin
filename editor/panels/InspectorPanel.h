#pragma once
#include <imgui.h>
#include <entt/entt.hpp>

namespace FaluEngine
{
	class Scene;
	struct MaterialAsset;
}

namespace Editor
{
	class InspectorPanel
	{
	public:
		void draw(FaluEngine::Scene* scene, entt::entity selected);

	private:
		void drawTransformComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawMeshComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawCameraComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawRigidbodyComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawScriptComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawLightComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawSkyComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawMaterialEditor(FaluEngine::MaterialAsset* material,
			const std::string& materialPath);

		void drawAddComponentMenu(FaluEngine::Scene* scene, entt::entity entity);
	};
}
