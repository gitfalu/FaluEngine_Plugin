#include "SpriteEditorPanel.h"

#include "asset/AssetManager.h"
#include "asset/loaders/TextureLoader.h"
#include "core/Logger.h"
#include "core/PathResolver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace Editor
{
	using namespace FaluEngine;
	namespace SS = FaluEngine::SpriteSlicer;

	namespace
	{
		constexpr float kHnadleScreenPx = 6.0f; // ハンドル当たり幅(画面px)
		constexpr float kMinZoom = 0.05f;
		constexpr float kMaxZoom = 64.0f;
		consteval float kSidePanelWidth = 320.0f;

		const ImU32 kColSlice = IM_COL32(80, 200, 255, 255);
		const ImU32 kColSelected = IM_COL32(255, 210, 60, 255);
		const ImU32 kColPrimart = IM_COL32(255, 255, 255, 255);

		bool isTextureExt(const std::string& path)
		{
			std::string ext = std::filesystem::path(path).extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".dds";
		}
	} // Helper

	//======= 読み書き =============
	void SpriteEditorPanel::open(const std::string& assetPath)
	{
	}

	bool SpriteEditorPanel::load(const std::string& assetPath)
	{
		return false;
	}

	bool SpriteEditorPanel::save()
	{
		return false;
	}

	void SpriteEditorPanel::revert()
	{
	}

	void SpriteEditorPanel::refreshTexture()
	{
	}

	/*

	Undo/Redo

	*/
	void SpriteEditorPanel::pushUndo()
	{
	}

	void SpriteEditorPanel::undo()
	{
	}

	void SpriteEditorPanel::redo()
	{
	}

	/*
	
	編集操作

	*/
	void SpriteEditorPanel::applyAutoSlice()
	{
	}

	void SpriteEditorPanel::deleteSelected()
	{
	}


	/*
	
	座標変換
	
	*/
	ImVec2 SpriteEditorPanel::texToScreen(const ImVec2& origin, const glm::vec2& t) const
	{
		return ImVec2();
	}

	glm::vec2 SpriteEditorPanel::screenToTex(const ImVec2& origin, const ImVec2& s) const
	{
		return glm::vec2();
	}

	void SpriteEditorPanel::fitToView(const ImVec2& viewSize)
	{
	}

	/*
	
	描画
	
	*/
	void SpriteEditorPanel::draw()
	{
	}

	void SpriteEditorPanel::drawToolbar()
	{
	}

	void SpriteEditorPanel::drawCanvas(const ImVec2& size)
	{
	}

	void SpriteEditorPanel::drawSidePanel(const ImVec2& size)
	{
	}

	void SpriteEditorPanel::handleShortcuts()
	{
		if (ImGui::GetIO().WantTextInput) return;
		const ImGuiIO& io = ImGui::GetIO();
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) save();
		if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) undo();
		if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)))) redo();
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A))
		{
			m_selected.clear();
			for (int i = 0; i < static_cast<int>(m_sheet.slices.size()); ++i) m_selected.insert(i);
			m_primary = m_sheet.slices.empty() ? -1 : 0;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelected();
		if (ImGui::IsKeyPressed(ImGuiKey_F)) m_needFit = true;
	}

	void SpriteEditorPanel::drawUnsavedPopup()
	{
		if (m_askUnsaved)
		{
			ImGui::OpenPopup("Unsaved Changes##Sprite");
			m_askUnsaved = false;
		}

		if (ImGui::BeginPopupModal("Unsaved Changes##Sprite", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("'%s' has unsaved changes.", m_path.c_str());
			auto proceed = [&]() {
				if (!m_pendingOpen.empty()) { const auto p = m_pendingOpen; m_pendingOpen.clear(); m_dirty = false; load(p); }
				if (m_pendingClose) { m_pendingClose = false; m_visible = false; }
				ImGui::CloseCurrentPopup();
				};
			if (ImGui::Button("Save", { 100.0f,0.0f })) { if (save()) proceed(); else ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if (ImGui::Button("Discard", { 100.0f,0.0f })) { m_dirty = false; proceed(); }
			ImGui::SameLine();
			if (ImGui::Button("Cancel", { 100.0f,0.0f })) { m_pendingOpen.clear(); m_pendingClose = false; ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
	}

}