#pragma once 
#include <imgui.h>
#include <cstdint>
#include <entt/entt.hpp>

namespace FaluEngine
{
	class DX11Renderer;
	class Scene;
}

namespace Editor
{
	enum class GizmoMode
	{
		Translate,
		Rotate,
		Scale,
	};

	class SceneViewPanel
	{
	public:
		void beginFrame();
		void drawImage(FaluEngine::DX11Renderer* renderer);

		void drawGizmo(FaluEngine::Scene* scene, 
			entt::entity selected,FaluEngine::DX11Renderer* renderer);

		void endFrame();

		[[nodiscard]] bool isFocused() const noexcept { return m_focused; }
		[[nodiscard]] float getWindowPosX()const noexcept { return m_windowPos.x; }
		[[nodiscard]] float getWindowPosY()const noexcept { return m_windowPos.y; }
		[[nodiscard]] float getWidth() const noexcept { return m_width; }
		[[nodiscard]] float getHeight() const noexcept { return m_height; }
		[[nodiscard]] GizmoMode getMode() const noexcept { return m_mode; }

		void setMode(GizmoMode mode) { m_mode = mode; }

	private:
		float m_width = 1280.0f;
		float m_height = 720.0f;
		bool m_focused = false;
		GizmoMode m_mode = GizmoMode::Translate;

		ImVec2 m_windowPos = { 0.0f,0.0f };
	};
}
