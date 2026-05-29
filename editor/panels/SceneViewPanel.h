#pragma once 
#include <imgui.h>
#include <cstdint>

namespace FaluEngine
{
	class DX11Renderer;
	class Scene;
}

namespace Editor
{
	class SceneViewPanel
	{
	public:
		void beginFrame();
		void drawImage(FaluEngine::DX11Renderer* renderer);

		[[nodiscard]] bool isFocused() const noexcept { return m_focused; }
		[[nodiscard]] float getWidth() const noexcept { return m_width; }
		[[nodiscard]] float getHeight() const noexcept { return m_height; }

	private:
		float m_width = 1280.0f;
		float m_height = 720.0f;
		bool m_focused = false;
	};
}
