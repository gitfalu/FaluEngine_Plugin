#include "HierarchyPanel.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "scene/SceneManager.h"
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

		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##CategoryFilter", "Filter by category...",
			m_categoryFilter, sizeof(m_categoryFilter));
		ImGui::Separator();

		if (ImGui::BeginPopupContextWindow("HierarchyContext"))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
			{
				scene->createEntity("New Entity");
				FaluEngine::SceneManager::get().markDirty();
			}
			ImGui::EndPopup();
		}
		
		for (auto entity : scene->getRootOrder())
		{
			if (m_categoryFilter[0] != '\0')
			{
				auto& tag = scene->registry().get<FaluEngine::TagComponent>(entity);
				std::string cat = tag.category;
				std::string filter = m_categoryFilter;
				std::transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
				std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
				if (cat.find(filter) == std::string::npos) continue;
			}
			drawEntityNode(scene, entity);
		}

		if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
		{
			m_selected = entt::null;
		}

		// ドラッグ＆ドロップによる親子化解除
		ImVec2 remaining = ImGui::GetContentRegionAvail();
		if (remaining.y < 10.0f) remaining.y = 10.0f;
		ImGui::InvisibleButton("##HierarchyDrop", remaining);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
			{
				entt::entity dragged =
					*static_cast<const entt::entity*>(payload->Data);
				FaluEngine::Entity e(dragged, scene);
				e.removeParent();
				FaluEngine::SceneManager::get().markDirty();
			}
			ImGui::EndDragDropTarget();
		}


		ImGui::End();
	}

	void HierarchyPanel::drawEntityNode(FaluEngine::Scene* scene, entt::entity entity)
	{
		auto& tag = scene->registry().get<FaluEngine::TagComponent>(entity);
		auto& rel = scene->registry().get<FaluEngine::RelationshipComponent>(entity);

		bool hasChildren = !rel.children.empty();

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow |
			ImGuiTreeNodeFlags_SpanAvailWidth;

		if (!hasChildren)
			flags |= ImGuiTreeNodeFlags_Leaf;

		if (m_selected == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		bool opened = ImGui::TreeNodeEx(
			reinterpret_cast<void*>(static_cast<uint64_t>(entity)),
			flags,"%s",tag.name.c_str());

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			m_selected = entity;

		// ドラッグソース
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("ENTITY_DRAG", &entity, sizeof(entt::entity));
			ImGui::Text("Move: %s", tag.name.c_str());
			ImGui::EndDragDropSource();
		}

		// ドロップターゲット
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
			{
				entt::entity dragged = *static_cast<const entt::entity*>(payload->Data);

				// 自分自身は無視
				if (dragged != entity && !isDescendantOf(scene,dragged,entity))
				{
					FaluEngine::Entity child(dragged, scene);
					FaluEngine::Entity parent(entity, scene);
					child.setParent(parent);
					FaluEngine::SceneManager::get().markDirty();
				}
			}
			ImGui::EndDragDropTarget();
		}

		
		// 右クリックメニュー
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Create Child Entity"))
			{
				auto child = scene->createEntity("New Entity");
				child.setParent(FaluEngine::Entity(entity, scene));
			}

			FaluEngine::Entity e(entity, scene);
			if (e.hasParent())
			{
				if (ImGui::MenuItem("Unparent"))
				{
					e.removeParent();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Delete Entity"))
			{
				FaluEngine::Entity e(entity, scene);
				scene->destroyEntity(e);
				FaluEngine::SceneManager::get().markDirty();
				if (m_selected == entity) m_selected = entt::null;
			}
			ImGui::EndPopup();
		}

		if (opened)
		{
			for (auto child : rel.children)
				drawEntityNode(scene, child);
			ImGui::TreePop();
		}
	}

	bool HierarchyPanel::isDescendantOf(FaluEngine::Scene* scene, entt::entity ancestor, entt::entity node)
	{
		auto& reg = scene->registry();
		entt::entity current = node;
		while (current != entt::null && reg.all_of<FaluEngine::RelationshipComponent>(current))
		{
			auto& rel = reg.get<FaluEngine::RelationshipComponent>(current);
			if (rel.parent == ancestor) return true;
			current = rel.parent;
		}
		return false;
	}
}
