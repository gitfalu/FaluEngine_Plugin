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

	void UILayoutPanel::beginDrag(DragMode mode, UILayout::HandleDir handle, entt::entity e, const glm::vec2& mouseLayout)
	{
	}

	void UILayoutPanel::updateDrag(const glm::vec2& mouseLayout)
	{
	}

	void UILayoutPanel::endDrag(bool cancel)
	{
	}

	void UILayoutPanel::pushEdit(entt::entity e, const FaluEngine::RectTransformComponent& before)
	{
	}

	void UILayoutPanel::applyFields(entt::entity e, const FaluEngine::RectTransformComponent& src)
	{
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