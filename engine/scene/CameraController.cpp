#include "CameraController.h"
#include "core/InputManager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace FaluEngine {

	void CameraController::onUpdate(float deltaTime) 
	{
		auto& input = InputManager::get();

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

		if (input.isMouseButtonDown(MouseButton::Right)) {
			glm::vec2 delta = input.getMouseDelta();
			if (glm::length(delta) > 0.0f) {
				m_camera.setYaw(m_camera.getYaw() - delta.x * rotSpeed);
				m_camera.setPitch(m_camera.getPitch() - delta.y * rotSpeed);
			}
		}
	}

	void CameraController::onMouseMove(float x, float y)
	{
		(void)x;
		(void)y;
	}
}
