#pragma once
#include "Input.h"
#include <array>
#include <glm/glm.hpp>

namespace FaluEngine {
	class InputManager {
	public:
		static InputManager& get() {
			static InputManager instance;
			return instance;
		}

		void update();

		[[nodiscard]] bool isKeyDown(Key key) const noexcept;
		[[nodiscard]] bool isKeyPressed(Key key) const noexcept;
		[[nodiscard]] bool isKeyReleased(Key key) const noexcept;

		[[nodiscard]] bool isMouseButtonDown(MouseButton btn) const noexcept;
		[[nodiscard]] bool isMouseButtonPressed(MouseButton btn) const noexcept;
		[[nodiscard]] bool isMouseButtonReleased(MouseButton btn) const noexcept;

		[[nodiscard]] glm::vec2 getMousePosition() const noexcept { return m_mousePos; }
		[[nodiscard]] glm::vec2 getMouseDelta() const noexcept { return m_mouseDelta; }
		[[nodiscard]] float getScrollDelta() const noexcept { return m_scrollDelta; }

		void onKeyDown(uint16_t vkCode, bool repeat);
		void onKeyup(uint16_t vkCode);
		void onMouseMove(float x, float y);
		void onMouseButtonDown(MouseButton btn);
		void onMouseButtonUp(MouseButton btn);
		void onMouseScroll(float delta);

	private:
		InputManager() = default;

		static constexpr size_t KEY_COUNT = 256;
		static constexpr size_t MOUSE_COUNT = 3;

		std::array<bool, KEY_COUNT> m_keyState = {};
		std::array<bool, MOUSE_COUNT> m_mouseState = {};

		std::array<bool, KEY_COUNT> m_prevKeyState = {};
		std::array<bool, MOUSE_COUNT> m_prevMouseState = {};

		glm::vec2 m_mousePos = { 0.0f,0.0f };
		glm::vec2 m_prevMousePos = { 0.0f,0.0f };
		glm::vec2 m_mouseDelta = { 0.0f,0.0f };
		float m_scrollDelta = 0.0f;
		bool m_firstMouse = true;
	};
}
