#include "SceneViewPanel.h"
#include "renderer/dx11/DX11Renderer.h"
#include "scene/Scene.h"
#include <imgui.h>

namespace Editor
{
	void SceneViewPanel::beginFrame()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
		ImGui::Begin("Scene View");
		ImGui::PopStyleVar();

		m_focused = ImGui::IsWindowFocused() || ImGui::IsWindowHovered();

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

		ImGui::End();
	}
}
