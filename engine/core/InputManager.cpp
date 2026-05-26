#include "InputManager.h"

#include "EventBus.h"
#include "Logger.h"

namespace FaluEngine {
	void InputManager::update()
	{
		m_prevKeyState = m_keyState;
		m_prevMouseState = m_mouseState;

		m_mouseDelta = m_mousePos - m_prevMousePos;
		m_prevMousePos = m_mousePos;
		m_scrollDelta = 0.0f;
	}

	/*
	 * キー状態
	 */
	bool InputManager::isKeyDown(Key key) const noexcept
	{
		return m_keyState[static_cast<uint16_t>(key)];
	}
	bool InputManager::isKeyPressed(Key key) const noexcept
	{
		auto k = static_cast<uint16_t>(key);
		return m_keyState[k] && !m_prevKeyState[k];
	}
	bool InputManager::isKeyReleased(Key key) const noexcept
	{
		auto k = static_cast<uint16_t>(key);
		return !m_keyState[k] && m_prevKeyState[k];
	}

	/*
	* マウス状態
	*/
	bool InputManager::isMouseButtonDown(MouseButton btn) const noexcept
	{
		return m_mouseState[static_cast<uint8_t>(btn)];
	}
	bool InputManager::isMouseButtonPressed(MouseButton btn) const noexcept
	{
		auto b = static_cast<uint8_t>(btn);
		return m_mouseState[b] && !m_prevMouseState[b];
	}
	bool InputManager::isMouseButtonReleased(MouseButton btn) const noexcept
	{
		auto b = static_cast<uint8_t>(btn);
		return !m_mouseState[b] && m_prevMouseState[b];
	}

	/**
	* Win32からの通知
	*/
	void InputManager::onKeyDown(uint16_t vkCode, bool repeat)
	{
		if (vkCode >= KEY_COUNT) return;
		m_keyState[vkCode] = true;
		EventBus::get().publish(KeyPressedEvent{
			static_cast<Key>(vkCode),repeat
			});
	}

	void InputManager::onKeyup(uint16_t vkCode)
	{
		if (vkCode >= KEY_COUNT) return;
		m_keyState[vkCode] = false;
		EventBus::get().publish(KeyReleasedEvent{
			static_cast<Key>(vkCode) });
	}

	void InputManager::onMouseMove(float x, float y)
	{
		if (m_firstMouse) {
			m_mousePos = { x,y };
			m_prevMousePos = { x,y };
			m_firstMouse = false;
		}

		m_mousePos = { x,y };
		EventBus::get().publish(MouseMovedEvent{
			x,y,m_mouseDelta.x,m_mouseDelta.y });
	}

	void InputManager::onMouseButtonDown(MouseButton btn)
	{
		m_mouseState[static_cast<uint8_t>(btn)] = true;
		EventBus::get().publish(MouseButtonPressedEvent{ btn });
	}

	void InputManager::onMouseButtonUp(MouseButton btn)
	{
		m_mouseState[static_cast<uint8_t>(btn)] = false;
		EventBus::get().publish(MouseButtonReleasedEvent{ btn });
	}

	void InputManager::onMouseScroll(float delta)
	{
		m_scrollDelta = delta;
		EventBus::get().publish(MouseScrolledEvent{ delta });
	}
}
