#pragma once
#include "Camera.h"
#include <glm/glm.hpp>

namespace FaluEngine {
	class CameraController {
	public:
		explicit CameraController(Camera& camera) : m_camera(camera){}

		void onUpdate(float deltaTime);

		void onMouseButtonDown(int button) {
			(void)button;
		}

		void onMouseButtonUp(int button) {
			(void)button;
		}

		void onMouseMove(float x, float y);

		float moveSpeed = 5.0f;
		float rotSpeed = 0.15f;
		float sprintMult = 2.0f;
	private:
		Camera& m_camera;

		bool m_rightMouseDown = false;
		bool m_firstMouse = true;
		float m_lastMouseX = 0.0f;
		float m_lastMouseY = 0.0f;
	};
}
