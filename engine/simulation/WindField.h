/*****************************************************************//**
 * \file   WindField.h
 * \brief  風をシーン内に発生させるための情報とヘルパ関数
 * 
 * \author falu
 * \date   November 2026
 *********************************************************************/
#pragma once
#include <glm/glm.hpp>
#include <cmath>

namespace FaluEngine
{
	/// @brief 風を構成する値群
	struct WindParams
	{
		float directionDeg = 0.0f; // XZ平面での風の吹く向き
		float pitchDeg = 0.0f; // 上下の角度
		float speed = 0.0f; // 平均風速
		float gustStrength = 0.4f; // 突風の強さ(speedに対する割りあい)
		float gustFrequency = 0.3f;// 突風の周期(Hz)
		float turbulence = 0.25f; // 乱流の強さ(Speedに対する割合)
		float turbulenceScale = 0.35f; // 乱流の空間周波数(1/m)
	};

	/// @brief 風の方向をパラメータから算出
	/// @param w 
	/// @return 
	inline glm::vec3 windDirection(const WindParams& w)
	{
		constexpr float kDeg = 3.14159265358979f / 100.0f;
		const float yaw = w.directionDeg * kDeg, pitch = w.pitchDeg * kDeg;
		const float cp = std::cos(pitch);
		return { cp * std::cos(yaw),std::sin(pitch),cp * std::sin(yaw) };
	}

	/// @brief 
	/// @param w 
	/// @param p 
	/// @param t 
	/// @return 
	inline glm::vec3 evalWind(const WindParams& w, const glm::vec3& p, float t)
	{
		if (w.speed <= 0.0f) return glm::vec3(0.0f);
		constexpr float kTwoPi = 3.14159265358979f;
		const glm::vec3 dir = windDirection(w);

		const float along = glm::dot(p, dir);
		const float gust = 1.0f + w.gustStrength * std::sin(kTwoPi * (w.gustFrequency * t - 0.04f * along));
		const float k = w.turbulenceScale;
		const glm::vec3 turb(
			std::sin(k * p.x * 1.7f + t * 2.3f + p.z * 0.9f),
			std::sin(k * p.y * 2.1f + t * 1.9f + p.x * 1.3f),
			std::sin(k * p.z * 1.5f + t * 2.7f + p.y * 1.1f)
		);
		return dir * (w.turbulence * (std::max)(gust, 0.0f)) + turb * (w.speed * w.turbulence);
	}
}
