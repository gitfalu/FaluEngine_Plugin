#include "CameraController.h"
#include "core/InputManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace FaluEngine {

	void CameraController::onUpdate(float deltaTime) 
	{
		auto& input = InputManager::get();

		m_altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

		if (m_rightDown && !m_altDown) {
			lockCursor();

			float speed = moveSpeed * deltaTime;
			if (input.isKeyDown(Key::Shift))
				speed *= sprintMult;

			glm::vec3 pos = m_camera.getPosition();
			if (input.isKeyDown(Key::W))
				pos += m_camera.getFront() * speed;
			if(input.isKeyDown(Key::S))
				pos -= m_camera.getFront() * speed;
			if (input.isKeyDown(Key::A))
				pos += m_camera.getRight() * speed;
			if (input.isKeyDown(Key::D))
				pos -= m_camera.getRight() * speed;
			if (input.isKeyDown(Key::Q))
				pos -= glm::vec3(0.0f, 1.0f, 0.0f) * speed;
			if (input.isKeyDown(Key::E))
				pos += glm::vec3(0.0f, 1.0f, 0.0f) * speed;

			m_camera.setPosition(pos);

			m_orbitTarget = pos + m_camera.getFront() * m_orbitDistance;
			m_orbitYaw = m_camera.getYaw();
			m_orbitPitch = m_camera.getPitch();
		}
		else if (!m_isOrbiting && !m_middleDown)
		{
			unlockCursor();
		}

		if (m_isOrbiting)
		{
			updateOrbitCamera();
		}
	}

	void CameraController::onMouseButtonDown(int button)
	{
		m_altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

		if (button == 0 && m_altDown)
		{
			m_isOrbiting = true;

			m_orbitYaw = m_camera.getYaw() + 180.0f;
			m_orbitPitch = -m_camera.getPitch();
			m_orbitDistance = glm::length(
				m_camera.getPosition() - m_orbitTarget);
			if (m_orbitDistance < 0.1f) m_orbitDistance = 0.1f;
		}

		if (button == 1) { m_rightDown = true; m_firstMouse = true; }
		if (button == 2) { m_middleDown = true; m_firstMouse = true; }
	}

	void CameraController::onMouseButtonUp(int button)
	{
		if (button == 0) m_isOrbiting = false;
		if (button == 1) { m_rightDown = false; unlockCursor(); }
		if (button == 2) m_middleDown = false;
		m_firstMouse = true;
	}

	void CameraController::onMouseScroll(float delta)
	{
		if (m_rightDown)
		{
			m_currentMoveSpeed = std::clamp(
				m_currentMoveSpeed * (delta > 0 ? 1.1f : 0.9f), 0.1f, 100.0f
			);
			return;
		}

		float speed = zoomSpeed * m_orbitDistance * 0.1f;
		speed = (std::max)(speed, 0.1f);

		glm::vec3 pos = m_camera.getPosition();
		pos += m_camera.getFront() * delta * speed;
		m_camera.setPosition(pos);

		m_orbitDistance -= delta * speed;
		m_orbitDistance = (std::max)(m_orbitDistance, 0.1f);
		m_orbitTarget = pos + m_camera.getFront() * m_orbitDistance;

		m_orbitYaw = m_camera.getYaw() + 180.0f;
		m_orbitPitch = -m_camera.getPitch();

	}

	void FaluEngine::CameraController::focusOn(const glm::vec3& target, float distance)
	{
		m_orbitTarget = target;
		m_orbitDistance = distance;

		m_orbitYaw = m_camera.getYaw() + 180.0f;
		m_orbitPitch = -m_camera.getPitch();
		updateOrbitCamera();
	}

	void CameraController::resetMouseState()
	{
		m_rightDown = false;
		m_middleDown = false;
		m_isOrbiting = false;
		m_firstMouse = true;
		unlockCursor();
	}

	void CameraController::updateOrbitCamera()
	{
		float pitchRad = glm::radians(m_orbitPitch);
		float yawRad = glm::radians(m_orbitYaw);

		glm::vec3 offset;
		offset.x = m_orbitDistance * cosf(pitchRad) * cosf(yawRad);
		offset.y = m_orbitDistance * sinf(pitchRad);
		offset.z = m_orbitDistance * cosf(pitchRad) * sinf(yawRad);

		glm::vec3 newPos = m_orbitTarget + offset;
		m_camera.setPosition(newPos);

		glm::vec3 dir = glm::normalize(m_orbitTarget - newPos);
		float yaw = glm::degrees(atan2f(dir.z, dir.x));
		float pitch = glm::degrees(asinf(dir.y));
		m_camera.setYaw(yaw);
		m_camera.setPitch(pitch);
	}

	void CameraController::lockCursor()
	{
		if (m_cursorLocked) return;
		m_cursorLocked = true;

		POINT p;
		GetCursorPos(&p);
		m_lockedCursorX = p.x;
		m_lockedCursorY = p.y;

		RECT rect;
		rect.left = static_cast<LONG>(m_viewX);
		rect.top = static_cast<LONG>(m_viewY);
		rect.right = static_cast<LONG>(m_viewX + m_viewW);
		rect.bottom = static_cast<LONG>(m_viewY + m_viewH);
		ClipCursor(&rect);

		ShowCursor(FALSE);
	}

	void CameraController::unlockCursor()
	{
		if (!m_cursorLocked) return;
		m_cursorLocked = false;

		ClipCursor(nullptr);
		SetCursorPos(m_lockedCursorX, m_lockedCursorY);
		ShowCursor(TRUE);
	}

	void CameraController::onMouseMove(float x, float y)
	{
		if (!m_rightDown && !m_middleDown && !m_isOrbiting)
		{
			m_lastMouseX = x;
			m_lastMouseY = y;
			return;
		}

		if (m_firstMouse)
		{
			m_lastMouseX = x;
			m_lastMouseY = y;
			m_firstMouse = false;
			return;
		}

		if (m_skipNextMove)
		{
			m_skipNextMove = false;
			m_lastMouseX = x;
			m_lastMouseY = y;
			return;
		}

		float dx = x - m_lastMouseX;
		float dy = y - m_lastMouseY;
		m_lastMouseX = x;
		m_lastMouseY = y;

		if (m_cursorLocked)
		{
			dx = std::clamp(dx, -50.0f, 50.0f);
			dy = std::clamp(dy, -50.0f, 50.0f);
		}


		if (m_rightDown && !m_altDown)
		{
			m_camera.setYaw(m_camera.getYaw() - dx * rotSpeed);
			m_camera.setPitch(m_camera.getPitch() - dy * rotSpeed);

			m_orbitTarget = m_camera.getPosition() +
				m_camera.getFront() * m_orbitDistance;
		}

		if (m_isOrbiting)
		{
			m_orbitYaw += dx * rotSpeed;
			m_orbitPitch -= dy * rotSpeed;
			m_orbitPitch = std::clamp(m_orbitPitch, -89.0f, 89.0f);
			updateOrbitCamera();
		}

		if (m_middleDown)
		{
			float panDist = m_orbitDistance * panSpeed;
			glm::vec3 pos = m_camera.getPosition();
			pos -= m_camera.getRight() * dx * panDist;
			pos += m_camera.getUp() * dy * panDist;
			m_camera.setPosition(pos);

			m_orbitTarget -= m_camera.getRight() * dx * panDist;
			m_orbitTarget += m_camera.getUp() * dy * panDist;
		}
	}
}
