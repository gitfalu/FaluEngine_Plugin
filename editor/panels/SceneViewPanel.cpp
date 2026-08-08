#include "SceneViewPanel.h"
#include "renderer/dx11/DX11Renderer.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "scene/SceneManager.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <entt/entt.hpp>

#include <imgui.h>

namespace Editor
{
	void SceneViewPanel::beginFrame()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
		ImGui::Begin("Scene View");
		ImGui::PopStyleVar();

		m_focused = ImGui::IsWindowFocused() || ImGui::IsWindowHovered();
		m_windowPos = ImGui::GetWindowPos();

		ImVec2 size = ImGui::GetContentRegionAvail();
		if (size.x < 1.0f) size.x = 1.0f;
		if (size.y < 1.0f) size.y = 1.0f;
		m_width = size.x;
		m_height = size.y;

		ImGui::End();
	}

	void SceneViewPanel::drawImage(FaluEngine::DX11Renderer* renderer)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
		ImGui::Begin("Scene View");
		ImGui::PopStyleVar();
		
		// Gizmo Mode Change
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4.0f,4.0f });

		if (ImGui::RadioButton("T", m_mode == GizmoMode::Translate))
			m_mode = GizmoMode::Translate;
		ImGui::SameLine();
		if (ImGui::RadioButton("R", m_mode == GizmoMode::Rotate))
			m_mode = GizmoMode::Rotate;
		ImGui::SameLine();
		if (ImGui::RadioButton("S", m_mode == GizmoMode::Scale))
			m_mode = GizmoMode::Scale;

		ImGui::PopStyleVar();

		if (renderer)
		{
			if (auto* srv = renderer->getSceneSRV())
			{
				ImVec2 size = { m_width,m_height };
				ImGui::Image(
					reinterpret_cast<ImTextureID>(srv),
					size,
					{ 0.0f,0.0f }, { 1.0f,1.0f }
				);
			}
		}
	}
	void SceneViewPanel::drawGizmo(FaluEngine::Scene* scene, entt::entity selected,FaluEngine::DX11Renderer* renderer)
	{
		if (!scene || selected == entt::null) return;
		if (!scene->registry().all_of<FaluEngine::TransformComponent,
			FaluEngine::CameraComponent>(entt::null))
		{
			// Find the has CameraComponent
		}
	
		glm::mat4 view = renderer->getView();
		glm::mat4 projection = renderer->getProjection();


		ImGuizmo::SetOrthographic(false);

		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetRect(
			m_windowPos.x, m_windowPos.y, 
			m_width, m_height
		);

		if (!scene->registry().all_of<FaluEngine::TransformComponent>(selected))return;
		auto& transform = scene->registry().get<FaluEngine::TransformComponent>(selected);
		glm::mat4 worldMatrix = transform.worldMatrix;

		ImGuizmo::OPERATION operation;
		switch (m_mode)
		{
		case Editor::GizmoMode::Rotate:
			operation = ImGuizmo::ROTATE;
			break;
		case Editor::GizmoMode::Scale:
			operation = ImGuizmo::SCALE;
			break;
		default:
			operation = ImGuizmo::TRANSLATE;
			break;
		}

		
		ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(projection),
			operation,
			ImGuizmo::WORLD,
			glm::value_ptr(worldMatrix)
		);

		if (ImGuizmo::IsUsing()) {
			glm::vec3 translation, scale, skew;
			glm::vec4 perspective;
			glm::quat rotation;
			glm::decompose(worldMatrix, scale, rotation, translation, skew, perspective);


			auto& rel = scene->registry().get<FaluEngine::RelationshipComponent>(selected);
			if (rel.parent != entt::null)
			{
				auto& parentTransform = scene->registry()
					.get<FaluEngine::TransformComponent>(rel.parent);
				glm::mat4 parentInv = glm::inverse(parentTransform.worldMatrix);
				glm::mat4 local = parentInv * worldMatrix;
				glm::decompose(local, scale, rotation, translation, skew, perspective);
			}
			transform.position = translation;
			transform.rotation = glm::normalize(rotation);
			transform.scale = scale;

			FaluEngine::SceneManager::get().markDirty();
		}
	}

	void SceneViewPanel::endFrame()
	{
		ImGui::End();
	}

}
