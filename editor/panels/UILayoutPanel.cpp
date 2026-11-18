#include "UILayoutPanel.h"

#include "FaluEngine/Component.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Scene.h"
#include "asset/AssetManager.h"
#include "asset/loaders/TextureLoader.h"
#include "core/PathResolver.h"
#include "scene/SceneManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace Editor
{
	using namespace FaluEngine;

	namespace
	{
		constexpr float kHandlePx = 8.0f;
		constexpr float kRotationHandleOffsetPx = 28.0f;
		constexpr float kMinZoom = 0.05f;
		constexpr float kMaxZoom = 8.0f;
		constexpr float kSnapThresholdPx = 6.0f;

		// レイアウトテンプレート
		const UILayout::HandleDir kHandles[8] = {
			{-1,+1},{0,+1},{+1,+1},
			{-1, 0},	   {+1, 0},
			{-1,-1},{0,-1},{+1,-1}
		};

		/// @brief 矩形の形を維持したまま回転を行う
		/// @param v 
		/// @param deg 
		/// @return 
		glm::vec2 rotateDeg(const glm::vec2& v, float deg)
		{
			const float r = glm::radians(deg);
			const float c = std::cos(r), s = std::sin(r);
			return { v.x * c - v.y * s,v.x * s + v.y * c };
		}

		ImU32 toU32(const glm::vec4& c)
		{
			return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w));
		}

		bool isTextureExt(const std::string& path)
		{
			std::string ext = std::filesystem::path(path).extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {return static_cast<char>(std::tolower(c)); });
			return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".dds";
		}

		UILayout::RectGeom geomOf(const RectTransformComponent& rt)
		{
			return { rt.computedPosition,rt.computedSize,rt.computedRotation };
		}
	}

	/*
	
	描画
	
	*/

	void UILayoutPanel::draw(FaluEngine::Scene* scene, entt::entity& selected)
	{
		ImGui::SetNextWindowSize({ 960.0f,620.0f }, ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("UI Layout"))
		{
			ImGui::End();
			return;
		}
		if (!scene)
		{
			ImGui::TextDisabled("No scene");
			ImGui::End();
			return;
		}

		if (scene != m_scene)
		{
			m_scene = scene;
			m_reg = &scene->registry();
			m_canvas = entt::null;
			m_undo.clear(); m_redo.clear();
			m_drag = DragMode::None;
			m_needFit = true;
		}

		// 選択中の要素が属するCanvasに追従
		if (isUIElement(selected))
		{
			const entt::entity c = findCanvasOf(selected);
			if (c != entt::null && c != m_canvas) { m_canvas = c; m_needFit = true; }
		}
		if (m_canvas == entt::null || !m_reg->valid(m_canvas) || !m_reg->all_of<CanvasComponent>(m_canvas))
		{
			m_canvas = entt::null;
			for (auto e : m_reg->view<CanvasComponent>()) { m_canvas = e; break; }
			m_needFit = true;
		}

		drawToolbar(scene, selected);

		if (m_canvas == entt::null)
		{
			ImGui::Separator();
			ImGui::TextDisabled("This scene has no Canvas");
			if (ImGui::Button("Create Canvas")) selected = createElement(scene, ElementKind::Canvas, entt::null);
			ImGui::End();
			return;
		}

		// 編集用レイアウト
		auto& canvas = m_reg->get<CanvasComponent>(m_canvas);
		UILayout::layoutCanvas(*m_reg, m_canvas, previewSize(canvas));
		m_layoutSize = canvas.computedSize;

		const ImVec2 avail = ImGui::GetContentRegionAvail();
		drawView(scene, selected, { avail.x,(std::max)(avail.y,50.0f) });

		ImGui::End();
	}

	void UILayoutPanel::drawToolbar(FaluEngine::Scene* scene, entt::entity& selected)
	{
		//==== Select Canvas ====
		std::vector<entt::entity> canvases;
		for (auto e : m_reg->view<CanvasComponent>()) canvases.push_back(e);

		auto nameOf = [&](entt::entity e) ->std::string {
			if (auto* tag = m_reg->try_get<TagComponent>(e)) return tag->name;
			return "Canvas";
			};

		ImGui::SetNextItemWidth(140.0f);
		if (ImGui::BeginCombo("##Canvas", m_canvas != entt::null ? nameOf(m_canvas).c_str() : "(none)"))
		{
			for (auto c : canvases)
			{
				ImGui::PushID(static_cast<int>(entt::to_integral(c)));
				if (ImGui::Selectable(nameOf(c).c_str(), c == m_canvas)) { m_canvas = c; m_needFit = true; }
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		const char* previews[] = { "Reference","1920x1080","1280x720","1080x1920","Custom" };
		ImGui::SetNextItemWidth(110.0f);
		if (ImGui::Combo("##Preview", &m_previewMode, previews, 5)) m_needFit = true;
		if (m_previewMode == 4)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragInt2("##Custom", m_customSize, 1.0f, 1, 16384)) m_needFit = true;
		}

		ImGui::SameLine();
		if (ImGui::Button("Fit")) m_needFit = true;
		ImGui::SameLine();
		ImGui::Text("%.0f%%", m_zoom * 100.0f);

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::Checkbox("Snap", &m_snap);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(60.0f);
		ImGui::DragInt("##Grid", &m_grid, 0.2f, 1, 256);
		ImGui::SameLine();
		ImGui::Checkbox("Names", &m_showNames);
		ImGui::SameLine();
		ImGui::Checkbox("Bounds", &m_showBounds);
		ImGui::SameLine();
		ImGui::Checkbox("Anchors", &m_showAnchors);

		//===== 2段目:作成/アンカー/Undo =====
		if (ImGui::Button("Create"))
			ImGui::OpenPopup("##CreateUI");
		if (ImGui::BeginPopup("##CreateUI"))
		{
			if (ImGui::MenuItem("Canvas")) selected = createElement(scene, ElementKind::Canvas, selected);
			if (ImGui::MenuItem("Panel")) selected = createElement(scene, ElementKind::Panel, selected);
			if (ImGui::MenuItem("Image")) selected = createElement(scene, ElementKind::Image, selected);
			if (ImGui::MenuItem("Button")) selected = createElement(scene, ElementKind::Button, selected);
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::BeginDisabled(!isUIElement(selected));
		if (ImGui::Button("Anchor")) ImGui::OpenPopup("##AnchorPreset");
		ImGui::EndDisabled();
		drawAnchorPopup(selected);

		ImGui::SameLine();
		ImGui::BeginDisabled(m_undo.empty());
		if (ImGui::Button("Undo")) undo();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(m_redo.empty());
		if (ImGui::Button("Redo")) redo();
		ImGui::EndDisabled();

		if (m_canvas != entt::null && m_reg->valid(m_canvas))
		{
			const auto& c = m_reg->get<CanvasComponent>(m_canvas);
			ImGui::SameLine();
			ImGui::TextDisabled("Layout %.0fx%.0f scale %.2f%s", c.computedSize.x, c.computedSize.y, c.scaleFactor,
				c.enabled ? "" : "  (Canvas disabled)");
		}
		if (!m_status.empty()) ImGui::TextColored({ 0.7f,0.9f,1.0f,1.0f }, "%s", m_status.c_str());

	}

	void UILayoutPanel::drawView(FaluEngine::Scene* scene, entt::entity& selected, const ImVec2& size)
	{
		(void)scene;
		ImGui::BeginChild("##UIView", size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		m_viewOrigin = ImGui::GetCursorScreenPos();
		m_viewSize = ImGui::GetContentRegionAvail();
		if (m_viewSize.x < 8.0f || m_viewSize.y < 8.0f) { ImGui::EndChild(); return; }
		if (m_needFit) { fitToView(m_viewSize); m_needFit = false; }

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 viewEnd = { m_viewOrigin.x + m_viewSize.x,m_viewOrigin.y + m_viewSize.y };
		dl->PushClipRect(m_viewOrigin, viewEnd, true);
		dl->AddRectFilled(m_viewOrigin, viewEnd, IM_COL32(24, 24, 26, 255));

		//=== 入力候補 ====
		ImGui::InvisibleButton("##UIViewInput", m_viewSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		ImGuiIO& io = ImGui::GetIO();

		// ズーム
		if (hovered && io.MouseWheel != 0.0f)
		{
			const glm::vec2 before = toLayout(io.MousePos);
			m_zoom = std::clamp(m_zoom * (io.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f), kMinZoom, kMaxZoom);
			// toLayout == beforeになるようにpanを補正
			const float cx0 = m_viewOrigin.x + m_viewSize.x * 0.5f;
			const float cy0 = m_viewOrigin.y + m_viewSize.y * 0.5f;
			m_pan.x = io.MousePos.x - cx0 - before.x * m_zoom;
			m_pan.y = io.MousePos.y - cy0 + before.y * m_zoom;
		}
		// パン
		if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) ||
			(ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))))
		{
			m_pan.x += io.MouseDelta.x;
			m_pan.y += io.MouseDelta.y;
		}

		//===== Canvas =======
		{
			const glm::vec2 half = m_layoutSize * 0.5f;
			const ImVec2 a = toScreen({ -half.x,half.y });
			const ImVec2 b = toScreen({ half.x,-half.y });
			dl->AddRectFilled(a, b, IM_COL32(40, 40, 46, 255));

			if (m_snap && m_grid * m_zoom >= 8.0f)
			{
				const float g = static_cast<float>(m_grid);
				for (float x = std::ceil(-half.x / g) * g; x <= half.x; x += g)
					dl->AddLine(toScreen({ x,half.y }), toScreen({ x,-half.y }), IM_COL32(255, 255, 255, 10));
				for (float y = std::ceil(-half.y / g) * g; y <= half.y; y += g)
					dl->AddLine(toScreen({ -half.x,y }), toScreen({ half.x,y }), IM_COL32(255, 255, 255, 10));
			}
			dl->AddLine(toScreen({ 0.0f,half.y }), toScreen({ 0.0f,-half.y }), IM_COL32(255, 255, 255, 28));
			dl->AddLine(toScreen({ -half.x,0.0f }), toScreen({ half.x,0.0f }), IM_COL32(255, 255, 255, 28));
			dl->AddRect(a, b, IM_COL32(150, 150, 160, 255));
		}

		//===== 要素の描画 =======
		std::vector<entt::entity> order;
		UILayout::collectDrawOrder(*m_reg, m_canvas, order);

		auto quadScreen = [&](const RectTransformComponent& rt, ImVec2 out[4]) {
			const glm::vec2 h = rt.computedSize * 0.5f;
			const glm::vec2 local[4] = { {-h.x,h.y},{h.x,h.y},{h.x,-h.y},{-h.x,-h.y} }; // TL TR BR BL
			for (int i = 0; i < 4; ++i)
				out[i] = toScreen(rt.computedPosition + rotateDeg(local[i], rt.computedRotation));
			};

		for (auto e : order)
		{
			const auto& rt = m_reg->get<RectTransformComponent>(e);
			ImVec2 q[4];
			quadScreen(rt, q);

			if (auto* img = m_reg->try_get<ImageComponent>(e))
			{
				if (img->visible)
				{
					if (!img->texturePath.empty() && !img->cachedTexture)
						img->cachedTexture = AssetManager::get().load<TextureAsset>(img->texturePath);

					glm::vec4 col = img->color;
					if (auto* btn = m_reg->try_get<ButtonComponent>(e)) col = btn->normalColor; // 実行時の通常状態と同じ色

					if (img->cachedTexture && img->cachedTexture->srv)
					{
						const glm::vec4 uv = img->uvRect; // x,y,w,h
						dl->AddImageQuad(reinterpret_cast<ImTextureID>(img->cachedTexture->srv.Get()), q[0], q[1], q[2], q[3],
							{ uv.x,uv.y }, { uv.x + uv.z,uv.y }, { uv.x + uv.z,uv.y + uv.w }, { uv.x,uv.y + uv.w }, toU32(col));
					}
					else
					{
						dl->AddQuadFilled(q[0], q[1], q[2], q[3], toU32(col));
					}
				}
			}

			if (auto* tc = m_reg->try_get<TextComponent>(e))
			{
				const ImVec2 ts = ImGui::CalcTextSize(tc->text.c_str());
				const ImVec2 c = toScreen(rt.computedPosition);
				dl->AddText({ c.x - ts.x * 0.5f,c.y - ts.y * 0.5f }, toU32(tc->color), tc->text.c_str());
			}

			if (m_showBounds) dl->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(255, 255, 255, 40));
			if (m_showNames && (q[2].x - q[0].x) > 24.0f)
			{
				if (auto* tag = m_reg->try_get<TagComponent>(e))
					dl->AddText({ (std::min)(q[0].x,q[3].x) + 3.0f,(std::min)(q[0].y,q[1].y) + 2.0f }, 
						IM_COL32(200, 220, 255, 170), tag->name.c_str());
			}
		}

		//==== 選択要素 =======
		const bool selValid = isUIElement(selected);
		if (selValid && m_reg->valid(selected))
		{
			const auto& rt = m_reg->get<RectTransformComponent>(selected);
			ImVec2 q[4];
			quadScreen(rt, q);
			dl->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(255, 210, 60, 255), 2.0f);

			const glm::vec2 half = rt.computedSize * 0.5f;
			for (const auto& h : kHandles)
			{
				const ImVec2 p = toScreen(rt.computedPosition + rotateDeg({ half.x * h.sx,half.y * h.sy }, rt.computedRotation));
				dl->AddRectFilled({ p.x - 4.0f,p.y - 4.0f }, { p.x + 4.0f,p.y + 4.0f }, IM_COL32(255, 255, 255, 255));
				dl->AddRect({ p.x - 4.0f,p.y - 4.0f }, { p.x + 4.0f,p.y + 4.0f }, IM_COL32(0, 0, 0, 255));
			}
			// 回転ハンドル
			const ImVec2 topMid = toScreen(rt.computedPosition + rotateDeg({ 0.0f,half.y }, rt.computedRotation));
			const ImVec2 rotH = toScreen(rt.computedPosition + rotateDeg({ 0.0f,half.y + kRotationHandleOffsetPx / m_zoom }, rt.computedRotation));
			dl->AddLine(topMid, rotH, IM_COL32(255, 210, 60, 200));
			dl->AddCircleFilled(rotH, 5.0f, IM_COL32(255, 210, 60, 255));

			// pivot(=見た目の基準点) 
			const glm::vec2 pivotWorld = rt.computedPosition + rotateDeg(rt.computedSize * (glm::vec2(0.5f, 0.5f) - rt.pivot) * -1.0f, rt.computedRotation);
			const ImVec2 pv = toScreen(pivotWorld);
			dl->AddCircleFilled(pv, 3.5f, IM_COL32(255, 80, 80, 255));

			// アンカー
			if (m_showAnchors)
			{
				const auto pf = UILayout::parentFrameOf(*m_reg, selected);
				const glm::vec2 pMin = pf.pos - pf.size * 0.5f;
				const float xs[2] = { rt.anchorMin.x,rt.anchorMax.x };
				const float ys[2] = { rt.anchorMin.y,rt.anchorMax.y };
				for (float ax : xs) for (float ay : ys)
				{
					const glm::vec2 w = pf.pos + rotateDeg(pMin + pf.size * glm::vec2(ax, ay) - pf.pos, pf.rotation);
					const ImVec2 p = toScreen(w);
					dl->AddQuadFilled({ p.x,p.y - 5.0f }, { p.x + 5.0f, p.y }, { p.x,p.y + 5.0f }, { p.x - 5.0f,p.y }, IM_COL32(90, 200, 255, 255));
				}
			}
		}

		// スナップのガイド線
		if (m_guideX)dl->AddLine(toScreen({ m_guideXVal,m_layoutSize.y }), toScreen({ m_guideXVal,-m_layoutSize.y }), IM_COL32(255, 60, 200, 200));
		if (m_guideY)dl->AddLine(toScreen({ -m_layoutSize.x,m_guideYVal }), toScreen({ m_layoutSize.x,m_guideYVal }), IM_COL32(255, 60, 200, 200));

		//==== クリック/ドラッグ =======
		const glm::vec2 mouse = toLayout(io.MousePos);

		if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsKeyDown(ImGuiKey_Space) && m_drag == DragMode::None)
		{
			bool started = false;
			if (selValid)
			{
				const auto& rt = m_reg->get<RectTransformComponent>(selected);
				const glm::vec2 half = rt.computedSize * 0.5f;

				// 回転ハンドル
				const ImVec2 rotH = toScreen(rt.computedPosition + rotateDeg({ 0.0f,half.y + kRotationHandleOffsetPx / m_zoom }, rt.computedRotation));
				if (std::hypot(io.MousePos.x - rotH.x, io.MousePos.y - rotH.y) <= kHandlePx + 2.0f)
				{
					beginDrag(DragMode::Rotate, {}, selected, mouse);
					started = true;
				}
				// リサイズハンドル
				for (const auto& h : kHandles)
				{
					if (started) break;
					const ImVec2 p = toScreen(rt.computedPosition + rotateDeg({ half.x * h.sx,half.y * h.sy }, rt.computedRotation));
					if (std::hypot(io.MousePos.x - p.x, io.MousePos.y - p.y) < kHandlePx)
					{
						beginDrag(DragMode::Resize, h, selected, mouse);
						started = true;
					}
				}
			}
			if (!started)
			{
				const entt::entity picked = pickAt(mouse);
				if (picked != entt::null)
				{
					selected = picked;
					beginDrag(DragMode::Move, {}, picked, mouse);
				}
				else
				{
					selected = entt::null;
				}
			}
		}

		if (m_drag != DragMode::None)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) updateDrag(mouse);
			else endDrag(false);
		}

		// テクスチャのドロップ
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				const std::string path = PathResolver::normalizeAssetPath(static_cast<const char*>(payload->Data));
				const entt::entity target = pickAt(mouse);
				if (isTextureExt(path) && target != entt::null && m_reg->all_of<ImageComponent>(target))
				{
					auto& img = m_reg->get<ImageComponent>(target);
					img.texturePath = path;
					img.cachedTexture.reset();
					selected = target;
					markSceneDirty();
				}
			}
			ImGui::EndDragDropTarget();
		}

		dl->PopClipRect();
		ImGui::EndChild();

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) handleKeyboard(selected);
	}

	void UILayoutPanel::drawAnchorPopup(entt::entity selected)
	{
		if (!ImGui::BeginPopup("##AnchorPreset")) return;

		ImGui::TextDisabled("Anchor / Pivot preset (keeps the rect)");
		const char* rowNames[4] = { "Top","Middle","Bottom","Stretch" };
		const char* colNames[4] = { "Left","Center","Right","Stretch" };
		const UILayout::VAlign vs[4] = { UILayout::VAlign::Top,UILayout::VAlign::Middle,UILayout::VAlign::Bottom,UILayout::VAlign::Stretch };
		const UILayout::HAlign hs[4] = { UILayout::HAlign::Left,UILayout::HAlign::Center,UILayout::HAlign::Right,UILayout::HAlign::Stretch };

		for (int r = 0; r < 4; ++r)
		{
			for (int c = 0; c < 4; ++c)
			{
				if (c > 0) ImGui::SameLine();
				char label[48];
				std::snprintf(label, sizeof(label), "%c%c##a%d_%d", rowNames[r][0], colNames[c][0], r, c);
				if (ImGui::Button(label, { 34.0f,28.0f }) && isUIElement(selected))
				{
					const RectTransformComponent before = m_reg->get<RectTransformComponent>(selected);
					UILayout::applyAnchorPreset(*m_reg, selected, hs[c], vs[r]);
					pushEdit(selected, before);
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s / %s", rowNames[r], colNames[c]);
			}
		}
		ImGui::EndPopup();
	}


	void UILayoutPanel::handleKeyboard(entt::entity selected)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput) return;

		if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) { undo(); return; }
		if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)))) { redo(); return; }
		if (ImGui::IsKeyPressed(ImGuiKey_F)) m_needFit = true;
		if (ImGui::IsKeyPressed(ImGuiKey_Escape) && m_drag != DragMode::None) { endDrag(true); return; }

		// 矢印キーで微調整。ドラッグ中は無効
		if (m_drag == DragMode::None && isUIElement(selected))
		{
			glm::vec2 d(0.0f);
			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) d.x -= 1.0f;
			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) d.x += 1.0f;
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) d.y += 1.0f;
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) d.y -= 1.0f;
			if (d != glm::vec2(0.0f))
			{
				if (io.KeyShift) d *= 10.0f;
				const RectTransformComponent before = m_reg->get<RectTransformComponent>(selected);
				const auto g = geomOf(before);
				UILayout::setWorldRect(*m_reg, selected, g.center + d, g.size, g.rotation);
				pushEdit(selected, before);
			}
		}
	}

	/*
	
	要素の作成

	*/

	entt::entity UILayoutPanel::createElement(FaluEngine::Scene* scene, ElementKind kind, entt::entity selected)
	{
		if (!scene) return entt::null;

		if (kind == ElementKind::Canvas)
		{
			Entity c = scene->createEntity("Canvas");
			c.addComponent<CanvasComponent>();
			m_canvas = c;
			markSceneDirty();
			return c;
		}

		// 親：選択中のUI要素/Canvas->無ければ現在のCanvas->無ければCavnasを新規作成
		entt::entity parent = entt::null;
		if (selected != entt::null && m_reg->valid(selected) &&
			(m_reg->all_of<RectTransformComponent>(selected) || m_reg->all_of<CanvasComponent>(selected)))
			parent = selected;
		else if (m_canvas != entt::null && m_reg->valid(m_canvas))
			parent = m_canvas;
		else
			parent = createElement(scene, ElementKind::Canvas, entt::null);

		const char* name = kind == ElementKind::Panel ? "Panel" : kind == ElementKind::Image ? "Image" : "Button";
		Entity e = scene->createEntity(name);
		auto& rt = e.addComponent<RectTransformComponent>();
		auto& img = e.addComponent<ImageComponent>();
		switch (kind)
		{
		case Editor::UILayoutPanel::ElementKind::Panel:
			rt.sizeDelta = { 300.0f,200.0f };
			img.color = { 0.1f,0.1f,0.12f,0.7f };
			break;
		case Editor::UILayoutPanel::ElementKind::Button:
			rt.sizeDelta = { 160.0f,40.0f };
			e.addComponent<ButtonComponent>();
			break;
		default:
			rt.sizeDelta = { 100.0f,100.0f };
			break;
		}
		e.setParent(Entity(parent, scene));

		const entt::entity canvas = findCanvasOf(parent);
		if (canvas != entt::null) m_canvas = canvas;
		UILayout::relayout(*m_reg, e);
		markSceneDirty();
		return e;
	}

	/*
	
	ドラッグ

	*/

	void UILayoutPanel::beginDrag(DragMode mode, UILayout::HandleDir handle, entt::entity e, const glm::vec2& mouseLayout)
	{
		m_drag = mode;
		m_handle = handle;
		// ドラック開始時の情報をそれぞれのメンバに格納
		m_dragEntity = e;
		m_startRT = m_reg->get<RectTransformComponent>(e);
		m_startGeom = geomOf(m_startRT); // 初期ジオメトリ情報
		m_startMouse = mouseLayout; // マウス初期座標	
		m_startScreen = ImGui::GetIO().MousePos; // 初期マウスレイアウト内座標

		m_dragActive = false;
		m_startAngle = std::atan2(
			mouseLayout.y - m_startGeom.center.y, 
			mouseLayout.x - m_startGeom.center.x);
		m_guideX = m_guideY = false;
	}

	void UILayoutPanel::updateDrag(const glm::vec2& mouseLayout)
	{
		if (m_drag == DragMode::None || !isUIElement(m_dragEntity)) return; // 変更状態出なければ実行しない
		const ImGuiIO& io = ImGui::GetIO();

		// 単なるクリックで移動が発生しないように４ｐｘの閾値を作る
		if (!m_dragActive)
		{
			if (std::hypot(io.MousePos.x - m_startScreen.x, io.MousePos.y - m_startScreen.y) < 4.0f) return;
			m_dragActive = true;
		}

		const bool snapOn = m_snap && !io.KeyCtrl; // Ctrl:一時的にスナップ無効
		const float grid = static_cast<float>((std::max)(m_grid, 1));
		const bool unrotated = std::fabs(std::remainder(m_startGeom.rotation, 360.0f)) < 1e-3f;
		glm::vec2 delta = mouseLayout - m_startMouse;
		m_guideX = m_guideY = false;

		if (m_drag == DragMode::Move)
		{
			// ＸかＹどちらかの軸のみで移動を行うようにする
			if (io.KeyShift) { if (std::fabs(delta.x) > std::fabs(delta.y)) delta.y = 0.0f; else delta.x = 0.0f; } // 軸固定
			glm::vec2 center = m_startGeom.center + delta;

			if (snapOn && unrotated)
			{
				const auto pf = UILayout::parentFrameOf(*m_reg, m_dragEntity);
				const bool parentAxisAligned = std::fabs(std::remainder(pf.rotation, 360.0f)) < 1e-3f;
				const glm::vec2 rectMin = center - m_startGeom.size * 0.5f;
				// 親が回転している場合親の端には吸着できない(閾値を負の値にして無効化、グリッドのみ)
				const auto sr = UILayout::computeSnapOffset(rectMin, m_startGeom.size, pf.pos - pf.size * 0.5f, pf.size,
					grid, parentAxisAligned ? kSnapThresholdPx / m_zoom : -1.0f);
				center += sr.offset;
				m_guideX = sr.snappedX; m_guideXVal = sr.guideX;
				m_guideY = sr.snappedY; m_guideYVal = sr.guideY;
			}
			UILayout::setWorldRect(*m_reg, m_dragEntity, center, m_startGeom.size, m_startGeom.rotation);
		}
		else if(m_drag == DragMode::Resize)
		{
			const bool corner = m_handle.sx != 0 && m_handle.sy != 0;
			UILayout::RectGeom g = UILayout::resizeRect(m_startGeom, m_handle, delta, io.KeyAlt, io.KeyShift && corner, 1.0f);

			if (snapOn && unrotated && !io.KeyAlt)
			{
				// 動かしている辺だけをグリッドに吸着
				glm::vec2  lo = g.center - g.size * 0.5f, hi = g.center + g.size * 0.5f;
				auto snap = [&](float v) { return std::round(v / grid) * grid; };
				if (m_handle.sx > 0) hi.x = snap(hi.x); if (m_handle.sx < 0)lo.x = snap(lo.x);
				if (m_handle.sy > 0) hi.y = snap(hi.y); if (m_handle.sy < 0)lo.y = snap(lo.y);
				if (hi.x - lo.x >= 1.0f && hi.y - lo.y >= 1.0f)
				{
					g.size = hi - lo;
					g.center = (lo + hi) * 0.5f;
				}
			}
			UILayout::setWorldRect(*m_reg, m_dragEntity, g.center, g.size, g.rotation);
		}
		else if (m_drag == DragMode::Rotate)
		{
			const float angle = std::atan2(mouseLayout.y - m_startGeom.center.y, mouseLayout.x - m_startGeom.center.x);
			float rot = m_startGeom.rotation + glm::degrees(angle - m_startAngle);
			if (io.KeyShift) rot = std::round(rot / 15.0f) * 15.0f; // Shift:15度刻み
			UILayout::setWorldRect(*m_reg, m_dragEntity, m_startGeom.center, m_startGeom.size, rot);
		}
	}

	void UILayoutPanel::endDrag(bool cancel)
	{
		if (m_drag == DragMode::None) return;
		const entt::entity e = m_dragEntity;
		if (isUIElement(e))
		{
			if (cancel)
			{// 操作をキャンセルして開始時の情報に戻す
				applyFields(e,m_startRT);
			}
			else
			{
				const auto& now = m_reg->get<RectTransformComponent>(e);
				const bool changed = now.anchoredPos != m_startRT.anchoredPos || now.sizeDelta != m_startRT.sizeDelta ||
					now.rotation != m_startRT.rotation;
				if (changed) pushEdit(e, m_startRT); // 変更履歴を追加
			}
		}
		m_drag = DragMode::None;
		m_dragActive = false;
		m_dragEntity = entt::null;
		m_guideX = m_guideY = false;
	}

	/*
	
	Undo/Redo

	*/

	void UILayoutPanel::pushEdit(entt::entity e, const FaluEngine::RectTransformComponent& before)
	{
		if (!isUIElement(e)) return;
		// 編集情報を移す
		Edit ed;
		ed.entity = e;
		ed.before = before;
		ed.after = m_reg->get<RectTransformComponent>(e);
		// 戻しリストに登録
		m_undo.push_back(ed);
		// 戻し回数の最大値を超えている場合一番古い変更を削除
		if (m_undo.size() > kMaxUndo) m_undo.erase(m_undo.begin());
		m_redo.clear();// やり直しリストを初期化
		markSceneDirty(); // 更新待ち状態に移行
	}

	void UILayoutPanel::applyFields(entt::entity e, const FaluEngine::RectTransformComponent& src)
	{
		if (!isUIElement(e)) return;
		// RectTransformに保持していた情報を適用
		auto& rt = m_reg->get<RectTransformComponent>(e);
		rt.anchorMin = src.anchorMin; rt.anchorMax = src.anchorMax;
		rt.anchoredPos = src.anchoredPos; rt.sizeDelta = src.sizeDelta;
		rt.pivot = src.pivot; rt.rotation = src.rotation; rt.scale = src.scale;
		UILayout::relayout(*m_reg, e); // 再レイアウト計算
		markSceneDirty(); // 更新待ち状態に移行
	}

	void UILayoutPanel::undo()
	{
		if (m_undo.empty()) return;
		// 変更履歴リストから最新の変更を抽出
		Edit ed = m_undo.back(); m_undo.pop_back();
		applyFields(ed.entity, ed.before); // 変更対象に変更前情報を適用
		m_redo.push_back(ed);// やり直しリストに戻した変更を追加
	}

	void UILayoutPanel::redo()
	{
		if (m_redo.empty()) return;
		// やり直しリストから最新の追加を抽出
		Edit ed = m_redo.back(); m_redo.pop_back();
		applyFields(ed.entity, ed.after); // 変更対象にやり直し情報を適用
		m_undo.push_back(ed); // 戻しリストに追加しなおす
	}

	/*
	
	座標変換・ヘルパ

	*/

	ImVec2 UILayoutPanel::toScreen(const glm::vec2& layout) const
	{
		const float cx = m_viewOrigin.x + m_viewSize.x * 0.5f + m_pan.x;
		const float cy = m_viewOrigin.y + m_viewSize.y * 0.5f + m_pan.y;
		return { cx + layout.x * m_zoom,cy - layout.y * m_zoom };
		
	}

	glm::vec2 UILayoutPanel::toLayout(const ImVec2& screen) const
	{
		const float cx = m_viewOrigin.x + m_viewSize.x * 0.5f + m_pan.x;
		const float cy = m_viewOrigin.y + m_viewSize.y * 0.5f + m_pan.y;
		return { (screen.x - cx) / m_zoom, -(screen.y - cy) / m_zoom };
	}

	bool UILayoutPanel::isUIElement(entt::entity e) const
	{
		return m_reg && e != entt::null && m_reg->valid(e) && m_reg->all_of<RectTransformComponent>(e);
	}

	entt::entity UILayoutPanel::findCanvasOf(entt::entity e) const
	{
		int guard = 0;
		while (m_reg && e != entt::null && m_reg->valid(e) && guard++ < 64)
		{
			if (m_reg->all_of<CanvasComponent>(e)) return e;
			auto* rel = m_reg->try_get<RelationshipComponent>(e);
			if (!rel) break;
			e = rel->parent;
		}

		return entt::null;
	}

	glm::vec2 UILayoutPanel::previewSize(const FaluEngine::CanvasComponent& canvas) const
	{
		switch (m_previewMode)
		{
		case 1: return { 1920.0f,1080.0f };
		case 2: return { 1280.0f,720.0f };
		case 3: return { 1080.0f,1920.0f };
		case 4: return { static_cast<float>((std::max)(m_customSize[0],1)),static_cast<float>((std::max)(m_customSize[1],1)) };
		default: return canvas.referenceResolution;
		}

		return glm::vec2();
	}


	entt::entity UILayoutPanel::pickAt(const glm::vec2& layoutPos) const
	{
		std::vector<entt::entity> order;
		UILayout::collectDrawOrder(*m_reg, m_canvas, order);
		for (auto it = order.rbegin(); it != order.rend(); ++it)
		{
			if (UILayout::hitTest(m_reg->get<RectTransformComponent>(*it), layoutPos)) return *it;
		}
		return entt::null;
	}


	void UILayoutPanel::fitToView(const ImVec2& viewSize)
	{
		const float w = (std::max)(m_layoutSize.x, 1.0f);
		const float h = (std::max)(m_layoutSize.y, 1.0f);
		m_zoom = std::clamp((std::min)((viewSize.x - 40.0f) / w, (viewSize.y - 40.0f) / h), kMinZoom, kMaxZoom);
		m_pan = { 0.0f,0.0f };
	}

	void UILayoutPanel::markSceneDirty()
	{
		FaluEngine::SceneManager::get().markDirty();
	}

}