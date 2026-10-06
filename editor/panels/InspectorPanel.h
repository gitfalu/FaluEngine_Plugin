#pragma once
#include "scene/SceneManager.h"
#include "FaluEngine/Scene.h"
#include <imgui.h>
#include <entt/entt.hpp>
#include <d3d11.h>
#include <glm/gtc/quaternion.hpp>

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
		void draw(FaluEngine::Scene* scene, entt::entity selected,ID3D11Device* device);

	private:
		void drawTransformComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawMeshComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawCameraComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawRigidbodyComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawTerrainComponent(FaluEngine::Scene* scene, entt::entity entity, ID3D11Device* device);
		void drawScriptComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawNativeScriptComponent(FaluEngine::Scene* scene, entt::entity entity);

		void drawLightComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawSkyComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawAudioComponent(FaluEngine::Scene* scene, entt::entity entity);

		//==== UI =====
		void drawRectTransformComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawCanvasComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawImageComponent(FaluEngine::Scene* scene, entt::entity entity);
		void drawButtonComponent(FaluEngine::Scene* scene, entt::entity entity);

		void drawMaterialEditor(FaluEngine::MaterialAsset* material,
			const std::string& materialPath);

		void drawAddComponentMenu(FaluEngine::Scene* scene, entt::entity entity, ID3D11Device* device);

		template<typename T>
		bool drawComponentHeader(const char* label, FaluEngine::Scene* scene, entt::entity entity)
		{
			if (!scene->registry().all_of<T>(entity)) return false;

			bool keepOpen = true;
			bool opened = ImGui::CollapsingHeader(label, &keepOpen, ImGuiTreeNodeFlags_DefaultOpen);

			if (!keepOpen)
			{
				scene->registry().remove<T>(entity);
				FaluEngine::SceneManager::get().markDirty();
				return false;
			}
			return opened;
		}
	};


}
