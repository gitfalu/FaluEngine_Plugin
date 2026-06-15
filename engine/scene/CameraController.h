#pragma once

#include "Camera.h"
#include <glm/glm.hpp>

namespace FaluEngine {
	class CameraController {
	public:
		explicit CameraController(Camera& camera) : m_camera(camera){}

		void onUpdate(float deltaTime);
		void onMouseMove(float x, float y);
		void onMouseButtonDown(int button);
		void onMouseButtonUp(int button);
		void onMouseScroll(float delta);
		void focusOn(const glm::vec3& target, float distance = 5.0f);
		void resetMouseState();

		void setViewRect(float x, float y, float w, float h)
		{
			m_viewX = x; m_viewY = y;
			m_viewW = w; m_viewH = h;
		}

		float moveSpeed = 5.0f;
		float rotSpeed = 0.15f;
		float panSpeed = 0.005f;
		float zoomSpeed = 0.5f;
		float sprintMult = 2.0f;

	private:
		Camera& m_camera;

		// マウスボタン
		bool m_rightDown = false;
		bool m_middleDown = false;
		bool m_altDown = false;
		bool  m_isOrbiting = false;
		bool m_skipNextMove = false;

		float m_lastMouseX = 0.0f;
		float m_lastMouseY = 0.0f;
		bool m_firstMouse = true;

		//-カーソルロック
		bool m_cursorLocked = false;
		int m_lockedCursorX = 0;
		int m_lockedCursorY = 0;

		// オービット
		glm::vec3 m_orbitTarget = { 0.0f,0.0f,0.0f };
		float m_orbitDistance = 5.0f;
		float m_orbitYaw = 90.0f;
		float m_orbitPitch = 20.0f;

		//-移動速度
		float m_currentMoveSpeed = 5.0f;

		//-SceneViewの矩形
		float m_viewX = 0.0f, m_viewY = 0.0f;
		float m_viewW = 1280.0f, m_viewH = 720.0f;

		void updateOrbitCamera();
		void lockCursor();
		void unlockCursor();
	};
}
