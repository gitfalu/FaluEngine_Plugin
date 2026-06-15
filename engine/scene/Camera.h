#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace FaluEngine {
	class Camera {
	public:
		Camera() { recalculate(); }

		void setPerspective(float fovDeg, float aspectRatio,
			float nearClip, float farClip) {
			m_fovDeg = fovDeg;
			m_aspectRatio = aspectRatio;
			m_nearClip = nearClip;
			m_farClip = farClip;
			m_projection = glm::perspectiveLH(
				glm::radians(fovDeg), aspectRatio, nearClip, farClip
			);
		}

		void lookAt(const glm::vec3& eye, const glm::vec3& target,
			const glm::vec3& up = { 0.0f,1.0f,0.0f }) {
			m_position = eye;
			m_view = glm::lookAtLH(eye, target, up);
		}

		void recalculate() {
			glm::vec3 front;
			front.x = cosf(glm::radians(m_yaw)) * cosf(glm::radians(m_pitch));
			front.y = sinf(glm::radians(m_pitch));
			front.z = sinf(glm::radians(m_yaw)) * cosf(glm::radians(m_pitch));

			m_front = glm::normalize(front);
			m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
			m_up = glm::normalize(glm::cross(m_right,m_front));
			m_view = glm::lookAtLH(m_position, m_position + m_front, m_up);
		}

		[[nodiscard]] const glm::mat4& getView() const noexcept { return m_view; }
		[[nodiscard]] const glm::mat4& getProjection() const noexcept { return m_projection; }
		[[nodiscard]] const glm::vec3& getPosition() const noexcept { return m_position; }
		[[nodiscard]] const glm::vec3& getFront() const noexcept { return m_front; }
		[[nodiscard]] const glm::vec3& getRight() const noexcept { return m_right; }
		[[nodiscard]] const glm::vec3& getUp() const noexcept { return m_up; }

		void setPosition(const glm::vec3& pos) { m_position = pos; recalculate(); }
		void setYaw(float yaw) { m_yaw = yaw; recalculate(); }
		void setPitch(float pitch) { m_pitch = glm::clamp(pitch, -89.0f, 89.0f); recalculate(); }

		[[nodiscard]] float getYaw() const noexcept { return m_yaw; }
		[[nodiscard]] float getPitch() const noexcept { return m_pitch; }
		[[nodiscard]] float getFovDeg() const noexcept { return m_fovDeg; }
		[[nodiscard]] float getAspectRatio() const noexcept { return m_aspectRatio; }
		[[nodiscard]] float getNearClip() const noexcept { return m_nearClip; }
		[[nodiscard]] float getFarClip() const noexcept { return m_farClip; }

	private:
		float m_fovDeg = 60.0f;
		float m_aspectRatio = 16.0f / 9.0f;
		float m_nearClip = 0.1f;
		float m_farClip = 1000.0f;

		glm::vec3 m_position = { 0.0f,0.0f,-3.0f };
		float m_yaw = 90.0f;
		float m_pitch = 0.0f;

		glm::mat4 m_view = glm::mat4(1.0f);
		glm::mat4 m_projection = glm::mat4(1.0f);
		glm::vec3 m_front = { 0.0f,0.0f,1.0f };
		glm::vec3 m_right = { 1.0f,0.0f,0.0f };
		glm::vec3 m_up = { 0.0f,1.0f,0.0f };
	};
}
