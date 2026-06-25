#pragma once
#include <imgui.h>

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

	private:
		float m_width = 1280.0f;
		float m_height = 720.0f;
	};
}
