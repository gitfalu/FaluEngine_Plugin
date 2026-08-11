#include "GameViewPanel.h"
#include "renderer/dx11/DX11Renderer.h"
#include <imgui.h>

namespace Editor
{
	void GameViewPanel::beginFrame()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
		ImGui::Begin("Game View");
		ImVec2 pos = ImGui::GetCursorScreenPos();
		m_screenPos = { pos.x,pos.y };

		ImVec2 size = ImGui::GetContentRegionAvail();
		if (size.x < 1.0f) 
			size.x = 1.0f;
		if (size.y < 1.0f) 
			size.y = 1.0f;
		m_width = size.x;
		m_height = size.y;

		ImVec2 mouse = ImGui::GetMousePos();
		m_localMousePos = { mouse.x - m_screenPos.x,mouse.y - m_screenPos.y };

		ImGui::PopStyleVar();
		ImGui::End();
	}

	void GameViewPanel::drawImage(FaluEngine::DX11Renderer* renderer)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
		ImGui::Begin("Game View");
		ImGui::PopStyleVar();

		if (renderer)
		{
			if (auto* srv = renderer->getGameSceneSRV()) {
				ImVec2 size = { m_width,m_height };
				ImGui::Image(reinterpret_cast<ImTextureID>(srv), size,
					{ 0.0f,0.0f }, { 1.0f,1.0f });
			}
			else
			{
				ImGui::TextDisabled("No primary camera in scene");
			}
		}
		ImGui::End();
	}
}
