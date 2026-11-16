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
		constexpr float kROtationHandleOffsetPx = 28.0f;
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


	void UILayoutPanel::draw(FaluEngine::Scene* scene, entt::entity& selected)
	{
	}

	void UILayoutPanel::drawToolbar(FaluEngine::Scene* scene, entt::entity& selected)
	{
	}

	void UILayoutPanel::drawView(FaluEngine::Scene* scene, entt::entity& selected, const ImVec2& size)
	{
	}

	void UILayoutPanel::drawAnchorPopup(entt::entity selected)
	{
	}

	void UILayoutPanel::handlwKeyboard(entt::entity selected)
	{
	}

	entt::entity UILayoutPanel::createElement(FaluEngine::Scene* scene, ElementKind kind, entt::entity selected)
	{
		return entt::entity();
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
		return { (layout.x - cx) / m_zoom,-(layout.y - cy) / m_zoom };
		
	}

	glm::vec2 UILayoutPanel::toLayout(const glm::vec2& screen) const
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