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
		constexpr float kHandleScreenPx = 6.0f; // ハンドル当たり幅(画面px)
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
		if (m_visible && m_dirty && assetPath != m_path)
		{
			m_pendingOpen = assetPath; // 未保存の変更があるので確認してから開く
			m_askUnsaved = true;
			return;
		}
		if (load(assetPath)) m_visible = true;
	}

	bool SpriteEditorPanel::load(const std::string& assetPath)
	{
		auto sheet = loadSpriteSheet(assetPath);
		if (!sheet)
		{
			m_status = "Failed to load: " + assetPath;
			LOG_ERROR("SpriteEditor: failed to load '{}'", assetPath);
			return false;
		}
		m_path = assetPath;
		m_sheet = *sheet;
		m_selected.clear();
		m_primary = -1;
		m_undoStack.clear();
		m_redoStack.clear();
		m_dirty = false;
		m_dragging = m_creating = false;
		m_needFit = true;
		m_loadedTexturePath.clear();
		m_texture.reset();
		m_status.clear();
		refreshTexture();

		return true;
	}

	bool SpriteEditorPanel::save()
	{
		if (m_path.empty()) return false;
		if (m_texture && m_texture->width > 0)
			m_sheet.textureSize = { static_cast<int>(m_texture->width),static_cast<int>(m_texture->height) };

		if (!saveSpriteSheet(m_sheet, m_path))
		{
			m_status = "Save failed: " + m_path;
			return false;
		}
		m_dirty = false;
		m_status = "Saved: " + m_path;
		return true;
	}

	void SpriteEditorPanel::revert()
	{
		if (!m_path.empty()) load(m_path);
	}

	void SpriteEditorPanel::refreshTexture()
	{
		if (m_sheet.texturePath == m_loadedTexturePath) return;
		m_loadedTexturePath = m_sheet.texturePath;
		m_texture.reset();
		if (m_sheet.texturePath.empty()) return;

		m_texture = AssetManager::get().load<TextureAsset>(m_sheet.texturePath);
		if (m_texture && m_texture->width > 0)
		{
			m_sheet.textureSize = { static_cast<int>(m_texture->width),static_cast<int>(m_texture->height) };
			m_needFit = true;
		}
	}

	/*

	Undo/Redo

	*/
	void SpriteEditorPanel::pushUndo()
	{
		m_undoStack.push_back(m_sheet.slices);
		if (m_undoStack.size() > kMaxUndo) m_undoStack.erase(m_undoStack.begin());
		m_redoStack.clear();
	}

	void SpriteEditorPanel::undo()
	{
		if (m_undoStack.empty()) return;
		m_redoStack.push_back(m_sheet.slices);
		m_sheet.slices = std::move(m_undoStack.back());
		m_undoStack.pop_back();
		m_selected.clear(); m_primary = -1;
		markDirty();
	}

	void SpriteEditorPanel::redo()
	{
		if (m_redoStack.empty()) return;
		m_undoStack.push_back(m_sheet.slices);
		m_sheet.slices = std::move(m_redoStack.back());
		m_redoStack.pop_back();
		m_selected.clear(); m_primary = -1;
		markDirty();
	}

	/*
	
	編集操作

	*/
	void SpriteEditorPanel::applyAutoSlice()
	{
		if (m_sheet.textureSize.x <= 0 || m_sheet.textureSize.y <= 0) { m_status = "Assign a texture first."; return; }

		std::vector<SpriteSlice> made;
		if (m_sliceMode == SliceMode::CellSize)
		{
			SS::CellGrid g;
			g.cell = { m_cellSize[0],m_cellSize[1] };
			g.offset = { m_offset[0],m_offset[1] };
			g.padding = { m_padding[0],m_padding[1] };
			made = SS::sliceByCellSize(m_sheet.textureSize, g, m_prefix);
		}
		else
		{
			made = SS::sliceByCount(m_sheet.textureSize, { m_count[0],m_count[1] }, { m_padding[0],m_padding[1] }, m_prefix);
		}
		if (made.empty()) { m_status = "No slices produced. Check the parameters."; return; }

		pushUndo();
		if (m_replaceOnSlice) m_sheet.slices.clear();
		for (auto& s : made)
		{
			s.name = SS::makeUniqueName(m_sheet.slices, s.name);
			m_sheet.slices.push_back(std::move(s));
		}

		m_selected.clear(); m_primary = -1;
		markDirty();
		m_status = std::to_string(made.size()) + " slices created.";
	}

	void SpriteEditorPanel::deleteSelected()
	{
		if (m_selected.empty()) return;
		pushUndo();
		// 後ろから消してインデックスのずれを避ける
		for (auto it = m_selected.rbegin(); it != m_selected.rend(); ++it)
			m_sheet.slices.erase(m_sheet.slices.begin() + *it);
		m_selected.clear(); m_primary = -1;
		markDirty();
		m_status = "Deleted. Clips reference slices by index: re-check clips that used this sheet.";
	}


	/*
	
	座標変換
	
	*/
	ImVec2 SpriteEditorPanel::texToScreen(const ImVec2& origin, const glm::vec2& t) const
	{
		return { origin.x + m_pan.x + t.x * m_zoom,origin.y + m_pan.y + t.y * m_zoom };
	}

	glm::vec2 SpriteEditorPanel::screenToTex(const ImVec2& origin, const ImVec2& s) const
	{
		return { (s.x - origin.x - m_pan.x) / m_zoom, (s.y - origin.y - m_pan.y) / m_zoom };
	}

	void SpriteEditorPanel::fitToView(const ImVec2& viewSize)
	{
		const float tw = static_cast<float>((std::max)(m_sheet.textureSize.x, 1));
		const float th = static_cast<float>((std::max)(m_sheet.textureSize.y, 1));
		m_zoom = std::clamp((std::min)((viewSize.x - 16.0f) / tw, (viewSize.y - 16.0f) / th), kMinZoom, kMaxZoom);
		m_pan = { (viewSize.x - tw * m_zoom) * 0.5f,(viewSize.y - th * m_zoom) * 0.5f };
	}

	/*
	
	描画
	
	*/
	void SpriteEditorPanel::draw()
	{
		if (!m_visible) { drawUnsavedPopup(); return; }

		bool open = true;
		ImGui::SetNextWindowSize({ 960.0f,600.0f }, ImGuiCond_FirstUseEver);
		const std::string title = "Sprite Editor###SpriteEditor";
		if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_editUndoPushed = false;
			refreshTexture();
			drawToolbar();

			const ImVec2 avail = ImGui::GetContentRegionAvail();
			const float canvasW = (std::max)(avail.x - kSidePanelWidth - ImGui::GetStyle().ItemSpacing.x, 100.0f);
			drawCanvas({ canvasW,avail.y });
			ImGui::SameLine();
			drawSidePanel({ kSidePanelWidth,avail.y });

			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) handleShortcuts();
		}
		ImGui::End();

		if (!open)
		{
			if (m_dirty) { m_pendingClose = true; m_askUnsaved = true; }
			else m_visible = false;
		}
		drawUnsavedPopup();
	}

	void SpriteEditorPanel::drawToolbar()
	{
		if (ImGui::Button("Save")) save();
		ImGui::SameLine();
		if (ImGui::Button("Revert")) revert();
		ImGui::SameLine();
		ImGui::BeginDisabled(m_undoStack.empty());
		if (ImGui::Button("Undo")) undo();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(m_redoStack.empty());
		if (ImGui::Button("Redo")) redo();
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		if (ImGui::RadioButton("Select", m_tool == Tool::Select)) m_tool = Tool::Select;
		ImGui::SameLine();
		if (ImGui::RadioButton("Create", m_tool == Tool::Create)) m_tool = Tool::Create;
		ImGui::SameLine();
		if (ImGui::Button("Fit")) m_needFit = true;
		ImGui::SameLine();
		ImGui::Text("%.0f%%", m_zoom * 100.0f);

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::Text("%s%s", m_path.c_str(), m_dirty ? " *" : "");
		if (!m_status.empty())
		{
			ImGui::TextColored({ 0.7f,0.9f,1.0f,1.0f }, "%s", m_status.c_str());
		}
	}

	void SpriteEditorPanel::drawCanvas(const ImVec2& size)
	{
		ImGui::BeginChild("##SpriteCanvas", size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 canvas = ImGui::GetContentRegionAvail();
		if (canvas.x < 4.0f || canvas.y < 4.0f) { ImGui::EndChild(); return; }

		if (m_needFit) { fitToView(canvas); m_needFit = false; }
		m_lastCanvasSize = canvas;
		m_canvasOrigin = origin;

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 canvasEnd = { origin.x + canvas.x,origin.y + canvas.y };
		dl->PushClipRect(origin, canvasEnd, true);

		// 背景
		dl->AddRectFilled(origin, canvasEnd, IM_COL32(38, 38, 40, 255));
		{
			const float cell = 16.0f;
			for (float y = 0; y < canvas.y; y += cell)
				for (float x = 0; x < canvas.x; x += cell)
					if ((static_cast<int>(x / cell) + static_cast<int>(y / cell)) % 2 == 0)
						dl->AddRectFilled({ origin.x + x,origin.y + y },
							{ (std::min)(origin.x + x + cell,canvasEnd.x),
							(std::min)(origin.y + y + cell,canvasEnd.y) },
							IM_COL32(48, 48, 50, 255));
		}

		// 入力領域
		ImGui::InvisibleButton("##CanvasInput", canvas,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		ImGuiIO& io = ImGui::GetIO();

		// テクスチャのドロップ
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				const std::string p = PathResolver::normalizeAssetPath(static_cast<const char*>(payload->Data));
				if (isTextureExt(p))
				{
					m_sheet.texturePath = p;
					markDirty();
					refreshTexture();
				}
			}
			ImGui::EndDragDropTarget();
		}

		const bool hasTex = m_texture && m_texture->srv && m_sheet.textureSize.x > 0;
		if (!hasTex)
		{
			dl->AddText({ origin.x + 16.0f,origin.y + 16.0f }, IM_COL32(200, 200, 200, 255),
				m_sheet.texturePath.empty() ? "Drop a texture here (or set it in the side panel)" :
				"Texture not found / not loaded");
			dl->PopClipRect();
			ImGui::EndChild();
			return;
		}

		// ズーム(カーソル位置を基準)
		if (hovered && io.MouseWheel != 0.0f)
		{
			const glm::vec2 before = screenToTex(origin, io.MousePos);
			m_zoom = std::clamp(m_zoom * (io.MouseWheel > 0.0f ? 1.15f : 1.0f / 1.15f), kMinZoom, kMaxZoom);
			m_pan.x = io.MousePos.x - origin.x - origin.x - before.x * m_zoom;
			m_pan.y = io.MousePos.y - origin.y - origin.y - before.x * m_zoom;
		}
		// パン(中ボタン/右ボタン)
		if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) ||
			ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)))
		{
			m_pan.x += io.MouseDelta.x;
			m_pan.y += io.MouseDelta.y;
		}

		// テクスチャ本体
		const float tw = static_cast<float>(m_sheet.textureSize.x);
		const float th = static_cast<float>(m_sheet.textureSize.y);
		const ImVec2 p0 = texToScreen(origin, { 0.0f,0.0f });
		const ImVec2 p1 = texToScreen(origin, { tw, th });
		dl->AddImage(reinterpret_cast<ImTextureID>(m_texture->srv.Get()), p0, p1);
		dl->AddRect(p0, p1, IM_COL32(120, 120, 120, 255));

		// ピクセルグリッド(十分拡大したときのみ)
		if (m_zoom >= 8.0f)
		{
			const glm::vec2 tl = screenToTex(origin, origin);
			const glm::vec2 br = screenToTex(origin, canvasEnd);
			const int x0 = (std::max)(0, static_cast<int>(std::floor(tl.x)));
			const int x1 = (std::min)(m_sheet.textureSize.x, static_cast<int>(std::ceil(br.x)));
			const int y0 = (std::max)(0, static_cast<int>(std::floor(tl.y)));
			const int y1 = (std::min)(m_sheet.textureSize.y, static_cast<int>(std::ceil(br.y)));
			for (int x = x0; x <= x1; ++x) dl->AddLine(texToScreen(origin, { (float)x,(float)y0 }), 
				texToScreen(origin, { (float)x,(float)y1 }), IM_COL32(255, 255, 255, 28));
			for (int y = y0; y <= y1; ++y) dl->AddLine(texToScreen(origin, { (float)x0,(float)y }), 
				texToScreen(origin, { (float)x1,(float)y }), IM_COL32(255, 255, 255, 28));
		}

		//=== 入力処理 =====
		const glm::vec2 mouseTex = screenToTex(origin, io.MousePos);
		const float handleTex = kHandleScreenPx / m_zoom;

		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsKeyDown(ImGuiKey_Space))
		{
			const bool additive = io.KeyCtrl;
			if (m_tool == Tool::Create)
			{
				m_creating = true;
				m_dragStartTex = mouseTex;
			}
			else
			{
				// 1)選択中スライスのハンドル/本体
				SS::Handle h = SS::Handle::None;
				int hitIndex = -1;
				if (m_primary >= 0 && m_primary < static_cast<int>(m_sheet.slices.size()))
				{
					h = SS::hitHandle(m_sheet.slices[m_primary].rect, mouseTex, handleTex);
					if (h != SS::Handle::None) hitIndex = m_primary;
				}
				// 2) それ以外のスライス(手前＝後ろのインデックスを優先)
				if (hitIndex < 0)
				{
					for (int i = static_cast<int>(m_sheet.slices.size()) - 1; i >= 0; --i)
					{
						const auto hh = SS::hitHandle(m_sheet.slices[i].rect, mouseTex, 0.0f);
						if (hh != SS::Handle::None) { hitIndex = i; h = SS::Handle::Body; break; }
					}
				}

				if (hitIndex >= 0)
				{
					if (additive)
					{
						if (m_selected.count(hitIndex)) m_selected.erase(hitIndex);
						else m_selected.insert(hitIndex);
						m_primary = hitIndex;
					}
					else if (!m_selected.count(hitIndex))
					{
						m_selected = { hitIndex };
						m_primary = hitIndex;
					}
					else m_primary = hitIndex;

					if (m_selected.count(hitIndex))
					{
						pushUndo();
						m_dragging = true;
						m_dragHandle = h;
						m_dragStartTex = mouseTex;
						m_dragOrig.clear();
						for (int i : m_selected) m_dragOrig.push_back({ i, m_sheet.slices[i].rect });
					}
				}
				else if (!additive)
				{
					m_selected.clear();
					m_primary = -1;
				}
			}
		}
		if (m_dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const glm::vec2 delta = mouseTex - m_dragStartTex;
			if (m_dragHandle == SS::Handle::Body)
			{
				for (auto& [idx, orig] : m_dragOrig)
					m_sheet.slices[idx].rect = SS::dragRect(orig, SS::Handle::Body, delta, m_sheet.textureSize);	
			}
			else
			{
				for (auto& [idx, orig] : m_dragOrig)
					if (idx == m_primary)
						m_sheet.slices[idx].rect = SS::dragRect(orig, m_dragHandle, delta, m_sheet.textureSize);
			}
			markDirty();
		}
		if (m_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			m_dragging = false;
			m_dragHandle = SS::Handle::None;
		}

		if (m_creating)
		{
			const glm::vec2 a = (glm::min)(m_dragStartTex, mouseTex);
			const glm::vec2 b = (glm::min)(m_dragStartTex, mouseTex);
			const glm::vec4 rect = SS::clampRect({ a.x,a.y,b.x - a.x,b.y - a.y },
				m_sheet.textureSize);
			dl->AddRect(texToScreen(origin, { rect.x,rect.y }),
				texToScreen(origin, { rect.x + rect.z,rect.y + rect.w }),
				IM_COL32(120, 255, 120, 255), 0.0f, 0, 2.0f);

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				m_creating = false;
				if (b.x - a.x >= 1.0f && b.y - a.y >= 1.0f)
				{
					pushUndo();
					SpriteSlice s;
					s.name = SS::makeUniqueName(m_sheet.slices, "sprite");
					s.rect = rect;
					m_sheet.slices.push_back(std::move(s));
					m_selected = { static_cast<int>(m_sheet.slices.size()) - 1 };
					m_primary = *m_selected.begin();
					markDirty();
				}
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_creating = false;
		}

		//===== スライス描画 ======
		for (int i = 0; i < static_cast<int>(m_sheet.slices.size()); ++i)
		{
			const auto& s = m_sheet.slices[i];
			const bool sel = m_selected.count(i) > 0;
			const ImVec2 a = texToScreen(origin, { s.rect.x,s.rect.y });
			const ImVec2 b = texToScreen(origin, { s.rect.x + s.rect.z,s.rect.y + s.rect.w });
			const ImU32 col = (i == m_primary) ? kColPrimart : sel ? kColSelected : kColSlice;
			dl->AddRect(a, b, col, 0.0f, 0, sel ? 2.0f : 1.0f);
			if (sel) dl->AddRectFilled(a, b, IM_COL32(255, 210, 60, 28));
			if (m_zoom >= 2.0f && (b.x - a.x) > 30.0f)
				dl->AddText({ a.x + 3.0f,a.y + 2.0f }, col, s.name.c_str());

			if (sel)
			{
				// pivot
				const ImVec2 pv = texToScreen(origin, 
					{ s.rect.x + s.rect.z * s.pivot.x,s.rect.y + s.rect.w * s.pivot.y });
				dl->AddCircleFilled(pv, 3.5f, IM_COL32(255, 80, 80, 255));
			}
			if (i == m_primary)
			{
				const float hs = 3.5f;
				const ImVec2 pts[8] = { a, { (a.x + b.x) * 0.5f,a.y},{b.x,a.y},{a.x,(a.y + b.y) * 0.5f},
					{b.x,(a.y + b.y) * 0.5f},{a.x,b.y},{(a.x,b.x) * 0.5f,b.y},b };
				for (const auto& p : pts) dl->AddRectFilled({ p.x - hs,p.y - hs }, { p.x + hs,p.y + hs }, IM_COL32(255, 255, 255, 255));
			}
		}

		dl->PopClipRect();
		ImGui::EndChild();
	}

	void SpriteEditorPanel::drawSidePanel(const ImVec2& size)
	{
		ImGui::BeginChild("##SpriteSide", size, true);

		//=== Textre ====
		ImGui::SeparatorText("Texture");
		char buf[512];
		std::strncpy(buf, m_sheet.texturePath.c_str(), sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		if (ImGui::InputText("##TexPath", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_sheet.texturePath = PathResolver::normalizeAssetPath(buf);
			markDirty();
			refreshTexture();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				const std::string p = PathResolver::normalizeAssetPath(static_cast<const char*>(payload->Data));
				if (isTextureExt(p)) { m_sheet.texturePath = p; markDirty(); refreshTexture(); }
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::TextDisabled("Size: %d x %d", m_sheet.textureSize.x, m_sheet.textureSize.y);

		//==== Auto Slice ==== 
		ImGui::SeparatorText("Auto Slice");
		int mode = static_cast<int>(m_sliceMode);
		ImGui::RadioButton("By Cell Size", &mode, 0); ImGui::SameLine();
		ImGui::RadioButton("By Count", &mode, 1);
		m_sliceMode = static_cast<SliceMode>(mode);

		if (m_sliceMode == SliceMode::CellSize)
		{
			ImGui::DragInt2("Cell", m_cellSize, 1.0f, 1, 8192);
			ImGui::DragInt2("Offset", m_offset, 1.0f, 0, 8192);
		}
		else
		{
			ImGui::DragInt2("Columns/Rows", m_count, 1.0f, 1, 512);
		}
		ImGui::DragInt2("Padding", m_padding, 1.0f, 0, 512);
		ImGui::InputText("Prefix", m_prefix, sizeof(m_prefix));
		ImGui::Checkbox("Replace existing slices", &m_replaceOnSlice);
		if (ImGui::Button("Slice", { -1.0f,0.0f })) applyAutoSlice();

		//==== Slices ==== 
		ImGui::SeparatorText("Slices");
		ImGui::Text("%d slices, %d selected", static_cast<int>(m_sheet.slices.size()),
			static_cast<int>(m_selected.size()));
		if (ImGui::BeginChild("##SliceList", { 0.0f,150.0f }, true))
		{
			for (int i = 0; i < static_cast<int>(m_sheet.slices.size()); ++i)
			{
				ImGui::PushID(i);
				char label[160];
				std::snprintf(label, sizeof(label), "%3d %s", i, m_sheet.slices[i].name.c_str());
				if (ImGui::Selectable(label, m_selected.count(i) > 0))
				{
					if (ImGui::GetIO().KeyCtrl)
					{
						if (m_selected.count(i)) m_selected.erase(i);
						else m_selected.insert(i);
					}
					else if (ImGui::GetIO().KeyShift && m_primary >= 0)
					{
						const int lo = (std::min)(m_primary, i), hi = (std::max)(m_primary, i);
						for (int k = lo; k <= hi; ++k) m_selected.insert(k);
					}
					else m_selected = { i };
					m_primary = i;
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::BeginDisabled(m_selected.empty());
		if (ImGui::Button("Delete")) deleteSelected();
		ImGui::EndDisabled();

		//=== 選択スライスのプロパティ ====
		if (m_primary >= 0 && m_primary < static_cast<int>(m_sheet.slices.size()))
		{
			ImGui::SeparatorText("Selected Slice");
			auto& s = m_sheet.slices[m_primary];

			char nameBuf[128];
			std::strncpy(nameBuf, s.name.c_str(), sizeof(nameBuf) - 1);
			nameBuf[sizeof(nameBuf) - 1] = '\0';
			if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				pushUndo();
				s.name = SS::makeUniqueName(m_sheet.slices, nameBuf, m_primary);
				markDirty();
			}

			// 連続ドラッグ中でもUndoは一回だけ積む(値を書き換える前に呼ぶこと)
			auto beginEdit = [&]() { if (!m_editUndoPushed) { pushUndo(); m_editUndoPushed = true; }};

			glm::vec4 r = s.rect;
			if (ImGui::DragFloat4("X Y W H", &r.x, 1.0f))
			{
				beginEdit();
				s.rect = SS::clampRect(r, m_sheet.textureSize);
				markDirty();
			}
			glm::vec2 pv = s.pivot;
			if (ImGui::DragFloat2("Pivot", &pv.x, 0.01f, 0.0f, 1.0f))
			{
				beginEdit();
				s.pivot = glm::clamp(pv, glm::vec2(0.0f), glm::vec2(1.0f));
				markDirty();
			}
			if (ImGui::SmallButton("Center")) { pushUndo(); s.pivot = { 0.5f,0.5f }; markDirty(); }
			ImGui::SameLine();
			if (ImGui::SmallButton("Bottom")) { pushUndo(); s.pivot = { 0.5f,1.0f }; markDirty(); }
			ImGui::SameLine();
			if (ImGui::SmallButton("Top Left")) { pushUndo(); s.pivot = { 0.0f,0.0f }; markDirty(); }
		}

		//=== Clip ====
		ImGui::SeparatorText("Create Clip");
		ImGui::DragFloat("FPS", &m_clipFps, 0.1f, 1.0f, 120.0f);
		ImGui::Checkbox("Loop", &m_clipLoop);
		ImGui::BeginDisabled(m_selected.empty() || m_path.empty());
		if (ImGui::Button("Create Clip from Selection", { -1.0f,0.0f }))
		{
			// 保存済みの内容と食い違わないよう先にシートを保存する
			if(m_dirty && !save()) { /*save()がstatusを設定済み*/ }
			else
			{
				std::filesystem::path sheetFs = PathResolver::resolve(m_path);
				std::filesystem::path dir = sheetFs.parent_path();
				const std::string stem = sheetFs.stem().string();
				std::filesystem::path out;
				for (int i = 0; i < 1000; ++i)
				{
					out = dir / (stem + "_clip" + (i ? "_" + std::to_string(i) : "") + ".fanim2d");
					if (!std::filesystem::exists(out)) break;
				}
				const std::string outAsset = PathResolver::toAssetPath(out);
				std::vector<int> idx(m_selected.begin(), m_selected.end());
				auto clip = SS::makeClipFromSlices(m_path, idx, m_clipFps, m_clipLoop);
				m_status = saveSpriteClip(clip, outAsset) ? "Clip created: " + outAsset : "Failed to create clip.";
			}
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled("Frames follow slice index order.");

		ImGui::EndChild();
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