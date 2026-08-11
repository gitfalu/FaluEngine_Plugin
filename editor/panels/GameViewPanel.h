#pragma once
#include <imgui.h>
#include <glm/glm.hpp>

namespace FaluEngine
{
	class DX11Renderer;
	class Scene;
}

namespace Editor
{
	class GameViewPanel
	{
	public:
		void beginFrame();
		void drawImage(FaluEngine::DX11Renderer* renderer);

		[[nodiscard]] float getWidth() const noexcept { return m_width; }
		[[nodiscard]] float getHeight() const noexcept { return m_height; }

		[[nodiscard]] glm::vec2 getScreenPos() const noexcept { return m_screenPos; }
		[[nodiscard]] glm::vec2 getLocalMousePos() const noexcept { return m_localMousePos; }
	private:
		float m_width = 1280.0f;
		float m_height = 720.0f;

		glm::vec2 m_screenPos = { 0.0f,0.0f };
		glm::vec2 m_localMousePos = { -1.0f,-1.0f };
	};
}
