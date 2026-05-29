#include "HierarchyPanel.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include <imgui.h>

namespace Editor
{
	void HierarchyPanel::draw(FaluEngine::Scene* scene)
	{
		ImGui::Begin("Hierarchy");

		if (!scene)
		{
			ImGui::TextDisabled("No active scene");
			ImGui::End();
			return;
		}

		ImGui::TextColored({ 0.4f,0.8f,1.0f,1.0f }, "%s", scene->getName().c_str());
		ImGui::Separator();

		if (ImGui::BeginPopupContextWindow("HierarchyContext"))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
			{
				scene->createEntity("New Entity");
			}
			ImGui::EndPopup();
		}

		auto view = scene->registry().view<FaluEngine::TagComponent>();
		for (auto entity : view)
		{
			drawEntityNode(scene, entity);
		}

		if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
		{
			m_selected = entt::null;
		}

		ImGui::End();
	}

	void HierarchyPanel::drawEntityNode(FaluEngine::Scene* scene, entt::entity entity)
	{
		auto& tag = scene->registry().get<FaluEngine::TagComponent>(entity);

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth |
			ImGuiTreeNodeFlags_Leaf;

		if (m_selected == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		bool opened = ImGui::TreeNodeEx(
			reinterpret_cast<void*>(static_cast<uint64_t>(entity)),
			flags,"%s",tag.name.c_str());

		if (ImGui::IsItemClicked())
			m_selected = entity;

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Delete Entity"))
			{
				FaluEngine::Entity e(entity, scene);
				scene->destroyEntity(e);
				if (m_selected == entity) m_selected = entt::null;
			}
			ImGui::EndPopup();
		}

		if (opened)ImGui::TreePop();

	}
}
