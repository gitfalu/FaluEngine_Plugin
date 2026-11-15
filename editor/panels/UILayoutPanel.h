#pragma once
#include <imgui.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>

#include "FaluEngine/UITypes.h"
#include "scene/UILayout.h"

namespace FaluEngine { class Scene; }

namespace Editor
{
	namespace UILayout = FaluEngine::UILayout;

	/// @brief UIを2Dビューで見ながら編集するパネル。
	class UILayoutPanel
	{
	public:
		/// @brief 描画
		/// @param scene 
		/// @param selected 
		void draw(FaluEngine::Scene* scene, entt::entity& selected);

	private:
		enum class DragMode { None, Move, Resize, Rotate };
		enum class ElementKind { Canvas, Panel, Image, Button };

		struct Edit
		{
			entt::entity entity = entt::null; // 編集するエンティティ
			FaluEngine::RectTransformComponent before; // 変更前の情報
			FaluEngine::RectTransformComponent after; // 変更後の情報
		};

		//==== 描画 =====
		void drawToolbar(FaluEngine::Scene* scene, entt::entity& selected);
		void drawView(FaluEngine::Scene* scene, entt::entity& selected, const ImVec2& size);
		void drawAnchorPopup(entt::entity selected);
		void handlwKeyboard(entt::entity selected);

		//===== 編集 ======
		entt::entity createElement(FaluEngine::Scene* scene, ElementKind kind, entt::entity selected);
		void beginDrag(DragMode mode, UILayout::HandleDir handle, entt::entity e, const glm::vec2& mouseLayout);
		void updateDrag(const glm::vec2& mouseLayout);
		void endDrag(bool cancel);
		void pushEdit(entt::entity e, const FaluEngine::RectTransformComponent& before);
		void undo();
		void redo();
		void applyFields(entt::entity e, const FaluEngine::RectTransformComponent& src);
		void markSceneDirty();

		//===== Helper =====
		[[nodiscard]] ImVec2 toScreen(const glm::vec2& layout) const;
		[[nodiscard]] glm::vec2 toLayout(const glm::vec2& screen) const;
		[[nodiscard]] bool isUIElement(entt::entity e) const;
		[[nodiscard]] entt::entity findCanvasOf(entt::entity e) const;
		[[nodiscard]] glm::vec2 previewSize(const FaluEngine::CanvasComponent& canvas) const;
		[[nodiscard]] entt::entity pickAt(const glm::vec2& layoutPos) const;
		void fitToView(const ImVec2& viewSize);

		//===== 状態 ======
		FaluEngine::Scene* m_scene = nullptr;
		entt::registry* m_reg = nullptr;
		entt::entity m_canvas = entt::null;

		// View
		float m_zoom = 0.5f;
		ImVec2 m_pan = { 0.0f,0.0f };
		ImVec2 m_viewOrigin = { 0.0f,0.0f }; // ビューの原点(初期設定は左上)
		ImVec2 m_viewSize = { 0.0f,0.0f };
		glm::vec2 m_layoutSize = { 1920.0f,1080.0f };
		bool m_needFit = true;

		// settings
		int m_previewMode = 0; // 0:CanvasのReference Resolution, 1 :1920*1080 ,2:1280*720 ,3:1080*1920 ,4:Custom
		int m_customSize[2] = { 1920,1080 };
		bool m_snap = true;
		int m_grid = 8;
		bool m_showNames = true;
		bool m_showBounds = true;
		bool m_showAnchors = true;

		// drag
		DragMode m_drag = DragMode::None;
		UILayout::HandleDir m_handle;
		entt::entity m_dragEntity = entt::null; // ドラッグするエンティティ
		FaluEngine::RectTransformComponent m_startRT; // 開始時の情報
		UILayout::RectGeom m_startGeom; // 開始時のジオメトリ
		glm::vec2 m_startMouse = { 0.0f,0.0f }; // マウスの開始位置
		ImVec2 m_startScreen = { 0.0f,0.0f }; // ドラッグ開始時の画面座標
		bool m_dragActive = false; // 閾値を超えて実際にドラックが始まったか
		float m_startAngle = 0.0f;
		bool m_guideX = false, m_guideY = false;
		float m_guideXVal = 0.0f, m_guideYVal = 0.0f;

		// Undo/Redo
		static constexpr size_t kMaxUndo = 128;
		std::vector<Edit> m_undo;
		std::vector<Edit> m_redo;

		std::string m_status;
	};
}