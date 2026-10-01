#include "SceneViewPanel.h"
#include "renderer/dx11/DX11Renderer.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"
#include "scene/SceneManager.h"
#include "tools/TerrainSculptTool.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <entt/entt.hpp>

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
				m_imagePos = ImGui::GetCursorScreenPos();
				ImVec2 size = { m_width,m_height };
				ImGui::Image(
					reinterpret_cast<ImTextureID>(srv),
					size,
					{ 0.0f,0.0f }, { 1.0f,1.0f }
				);
				m_imageSize = size;
			}
		}
	}
	void SceneViewPanel::drawGizmo(FaluEngine::Scene* scene, entt::entity selected,FaluEngine::DX11Renderer* renderer)
	{
		ImGuizmo::BeginFrame();

		if (!scene || selected == entt::null) return;
		if (!scene->registry().all_of<FaluEngine::TransformComponent,
			FaluEngine::CameraComponent>(entt::null))
		{
			// Find the has CameraComponent
		}
		ImGuizmo::SetOrthographic(false);

		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetRect(
			m_imagePos.x,m_imagePos.y,
			m_imageSize.x,m_imageSize.y
		);
		
		// ImGuizmoを使用したグリッドの表示
		glm::mat4 view = renderer->getView();
		glm::mat4 projection = renderer->getProjection();

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
			transform.setRotationQuat(glm::normalize(rotation));
			transform.scale = scale;

			FaluEngine::SceneManager::get().markDirty();
		}

		

	}

	void SceneViewPanel::drawTerrainBrush(FaluEngine::DX11Renderer* renderer)
	{
		auto& tool = Editor::TerrainSculptTool::get();
		glm::vec3 center;
		if (!tool.enabled || !tool.getLastHit(center) || !renderer) return;

		glm::mat4 view = renderer->getView();
		glm::mat4 proj = renderer->getProjection();
		glm::mat4 viewProj = proj * view;

		auto worldToScreen = [&](const glm::vec3& worldPos, ImVec2& outScreen) -> bool
			{
				glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
				if (clip.w <= 0.0001f) return false;
				glm::vec3 ndc = glm::vec3(clip) / clip.w;
				outScreen =
				{
					m_imagePos.x + (ndc.x * 0.5f + 0.5f) * m_imageSize.x,
					m_imagePos.y + (1.0f - (ndc.x * 0.5f + 0.5f)) * m_imageSize.y
				};
				return true;
			};

		constexpr int kSegments = 48;
		std::vector<ImVec2> points;
		points.reserve(kSegments + 1);

		for (int i = 0; i <= kSegments; ++i)
		{
			float t = (static_cast<float>(i) / kSegments) * 6.2831853f;
			glm::vec3 worldPos = center + glm::vec3(std::cos(t), 0.0f, std::sin(t)) * tool.radius;

			ImVec2 screen;
			if (!worldToScreen(worldPos, screen)) { points.clear(); break; }
			points.push_back(screen);
		}

		if (points.size() > 1)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddPolyline(points.data(), static_cast<int>(points.size()),
				IM_COL32(80, 220, 120, 255), ImDrawFlags_Closed, 2.0f);
		}
	}

	void SceneViewPanel::endFrame()
	{
		ImGui::End();
	}

}
