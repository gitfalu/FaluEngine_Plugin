/*****************************************************************//**
 * \file   ClothSolver.h
 * \brief  布の動きを解決するための
 * 
 * \author falu
 * \date   November 2026
 *********************************************************************/
#pragma once
#include "WindField.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	enum class ClothPinMode : int {TopEdge = 0,TopCorner = 1,None = 2};

	struct ClothSphere
	{
		glm::vec3 center{ 0.0f };
		float radius = 0.0f; // 0以下は無効
	};

	/// @brief 布が動く際のパラメータ
	struct ClothParams
	{
		int substeps = 4;
		int iterations = 6;
		float stiffness = 1.0f; // 構造/せん断拘束の強さ
		float bendStiffness = 0.3f; // 曲げ拘束の強さ
		float damping = 0.2f; // 速度減衰
		float gravity = 9.8f;
		float airDrag = 0.25f; // 空気抵抗
		float areaDensity = 0.5f; // 面密度
		float thickness = 0.02f; // 衝突の厚み
		bool groundEnabled = false;
		float groundY = 0.0f;
		float friction = 0.5f; // 接触時速度減衰割合
		float maxSpeed = 50.0f;
	};

	/// @brief 布の動きをシミュレーションするための公開クラス
	class FALU_SCENE_API ClothSim
	{
	public:
		void build(uint32_t nx, uint32_t ny, float width, float height,
			ClothPinMode pin, const glm::mat4& world);

		void updatePins(const glm::mat4& world);

		void step(float dt, const ClothParams& params, const WindParams& wind, float time,
			const ClothSphere* spheres, int sphereCount);

		void reset(const glm::mat4& world);

		[[nodiscard]] uint32_t nx() const noexcept { return m_nx; }
		[[nodiscard]] uint32_t ny() const noexcept { return m_ny; }
		[[nodiscard]] const std::vector<glm::vec3>& positions() const noexcept { return m_pos; }
		[[nodiscard]] const std::vector<uint32_t>& triangles() const noexcept { return m_tris; }

		[[nodiscard]] float maxStretch() const;

	private:
		struct Edge 
		{
			uint32_t a, b; float rest; bool bend;
		};

		uint32_t m_nx = 0, m_ny = 0;
		std::vector<glm::vec3> m_pos, m_prev, m_vel, m_restLocal, m_pinPrev, m_pinTarget;
		std::vector<float> m_invMass;
		std::vector<uint8_t> m_contact;
		std::vector<Edge> m_edges;
		std::vector<uint32_t> m_tris;
		std::vector<glm::vec3> m_force;
		float m_cellArea = 1.0f; // 粒子一個当たりの面積
	};

	std::vector<uint32_t> buildClothIndices(uint32_t nx, uint32_t ny, bool doubleSided);

	template<class V>
	void buildClothVertices(const ClothSim& sim, const glm::mat4& invWorld,
		float uvTiling, bool doubleSided,
		std::vector<glm::vec3>& scratchPos, std::vector<glm::vec3>& scratchNrm,
		std::vector<V>& out)
	{
		const uint32_t nx = sim.nx(), ny = sim.ny();

		if (nx < 2 || ny < 2) return;
		const size_t n = static_cast<size_t>(nx) * ny;
		scratchPos.resize(n);
		scratchNrm.resize(n,glm::vec3(0.0f));
		for (size_t i = 0; i < n; ++i)
			scratchPos[i] = glm::vec3(invWorld * glm::vec4(sim.positions()[i], 1.0f));

		const auto& tri = sim.triangles();
		for (size_t t = 0; t + 2 < tri.size(); t += 3)
		{
			const glm::vec3 fn = glm::cross(scratchPos[tri[t + 1]] - scratchPos[tri[t]],
				scratchPos[tri[t + 2]] - scratchPos[tri[t]]);
			scratchNrm[tri[t]] += fn; scratchNrm[tri[t + 1]] += fn; scratchNrm[tri[t + 2]] += fn;
		}

		out.resize(doubleSided ? n * 2 : n);
		for(uint32_t j = 0 ;j < ny;++j)
			for (uint32_t i = 0; i < nx; ++i)
			{
				const size_t k = static_cast<size_t>(j) * nx + i;
				const float l = glm::length(scratchNrm[k]);
				const glm::vec3 nrm = l > 1e-10f ? scratchNrm[k] / l : glm::vec3(0.0f, 0.0f, -1.0f);
				const uint32_t il = i > 0 ? i - 1 : i, ir = i + 1 < nx ? i + 1 : i;
				glm::vec3 tan = scratchPos[static_cast<size_t>(j) * nx + ir] - scratchPos[static_cast<size_t>(j) * nx + i];
				const float tl = glm::length(tan);
				tan = tl > 1e-10f ? tan / tl : glm::vec3(1.0f, 0.0f, 0.0f);

				V& v = out[k];
				v.position = scratchPos[k];
				v.color = { 1.0f,1.0f,1.0f,1.0f };
				v.uv = { uvTiling * static_cast<float>(i) / static_cast<float>(nx - 1),
				uvTiling * static_cast<float>(j) / static_cast<float>(ny - 1) };
				v.normal = nrm;
				v.tangent = tan;
				v.bitangent = glm::cross(nrm, tan);
				if (doubleSided)
				{
					V& b = out[n + k];
					b = v;
					b.normal = -nrm;
					b.bitangent = glm::cross(b.normal, tan);
				}
			}
	}
}
