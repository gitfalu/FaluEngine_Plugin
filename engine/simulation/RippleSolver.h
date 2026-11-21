/*****************************************************************//**
 * \file   RippleSolver.h
 * \brief  波紋/波のソルバー。D3D・EnTTに依存しない純粋な計算のみを行うスクリプト
 * 
 * \author tsunn
 * \date   November 2026
 *********************************************************************/
#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	/// @brief うねりの計算
	struct GerstnerWave
	{
		float amplitude = 0.0f; // 0で無効
		float wavelength = 10.0f;
		float directionDeg = 0.0f; // XZ平面上の進行方向
		float steepness = 0.5f; // 0..1
	};

	struct RippleSolverParams
	{
		float waveSpeed = 4.0f; // 波の伝達速度
		float damping = 0.6f; // 減衰
		float maxHeight = 1.5f; // 高さの絶対値上限
		int absorbWidth = 8; // 端の吸収層の幅(セル)
	};

	class FALU_SCENE_API RippleGrid
	{
	public:
		void resize(uint32_t nx, uint32_t nz, float sizeX, float sizeZ);
		void clear();

		/// @brief dt秒進める。CFL条件を満たすよう内部でサブステップ分割する。
		/// @param dt 
		/// @param p 
		void step(float dt, const RippleSolverParams& p);

		void addImpulse(float localX, float localZ, float strength, float radius, float maxHieght);

		[[nodiscard]] uint32_t nx() const noexcept { return m_nx; }
		[[nodiscard]] uint32_t nz() const noexcept { return m_nz; }
		[[nodiscard]] float dx() const noexcept { return m_dx; }
		[[nodiscard]] float dz() const noexcept { return m_dz; }
		[[nodiscard]] float sizeX() const noexcept { return m_sizeX; }
		[[nodiscard]] float sizeZ() const noexcept { return m_sizeZ; }
		[[nodiscard]] int lastSubSteps() const noexcept { return m_lastSubSteps; }
		[[nodiscard]] const std::vector<float>& heights() const noexcept { return m_h; }
		[[nodiscard]] const std::vector<float>& velocities() const noexcept { return m_v; }

	private:
		uint32_t m_nx = 0, m_nz = 0;
		float m_sizeX = 1.0f, m_sizeZ = 1.0f, m_dx = 1.0f, m_dz = 1.0f;
		int m_lastSubSteps = 0;
		std::vector<float> m_h, m_v;
	};

	//============ Gerstner =====================
	inline glm::vec3 evalGerstner(const GerstnerWave* waves, int count,
		float time,float x,float z)
	{
		constexpr float kPi = 3.14159265358979f;
		constexpr float kGravity = 9.8f;
		glm::vec3 d(0.0f);
		for (int i = 0; i < count; ++i)
		{
			const GerstnerWave& w = waves[i];
			if (w.amplitude <= 0.0f || w.wavelength <= 0.0001f) continue;

			const float k = 2.0f * kPi / w.wavelength;
			const float omega = std::sqrt(kGravity * k);
			const float rad = w.directionDeg * (kPi / 180.0f);
			const float dirX = std::cos(rad), dirZ = std::sin(rad);
			const float phase = k * (dirX * x + dirZ * z) - omega * time;
			const float s = std::sin(phase), c = std::cos(phase);
			// Q*k＊A <= 1 でとがりすぎを防ぐ
			const float q = (std::min)(w.steepness, 1.0f / (std::max)(k * w.amplitude, 1e-4f));
			d.x += q * w.amplitude * dirX * c;
			d.z += q * w.amplitude * dirZ * c;
			d.y += w.amplitude * s;
		}
		return d;
	}

	inline std::vector<uint32_t> buildGridIndices(uint32_t nx,uint32_t nz)
	{
		std::vector<uint32_t> idx;
		if (nx < 2 || nz < 2) return idx;
		idx.reserve(static_cast<size_t>(nx - 1) * (nz - 1) * 6);
		for(uint32_t z = 0; z + 1 < nz; ++z) 
			for (uint32_t x = 0; x + 1 < nx; ++x)
			{
				const uint32_t i0 = z * nx + x, i1 = z * nx + x + 1;
				const uint32_t i2 = (z + 1) * nx + x, i3 = (z + 1) * nx + x + 1;
				// TerraubGeenratorと同じ巻き順
				idx.push_back(i0); idx.push_back(i2); idx.push_back(i1);
				idx.push_back(i1); idx.push_back(i2); idx.push_back(i3);
			}
		return idx;
	}

	template<class V>
	void buildRippleVertices(const RippleGrid& grid,
		const GerstnerWave* waves, int waveCount, float time,
		bool centered, float uvTiling,
		std::vector<glm::vec3>& scratch, std::vector<V>& out)
	{
		const uint32_t nx = grid.nx(), nz = grid.nz();
		if (nx < 2 || nz < 2) return;
		out.resize(static_cast<size_t>(nx) * nz);
		scratch.resize(out.size());

		const float ox = centered ? -0.5f * grid.sizeX() : 0.0f;
		const float oz = centered ? -0.5f * grid.sizeZ() : 0.0f;
		const auto& h = grid.heights();

		for (uint32_t z = 0; z < nz; ++z)
			for (uint32_t x = 0; x < nx; ++x)
			{
				const size_t i = static_cast<size_t>(z) * nx + x;
				const float bx = ox + x * grid.dx();
				const float bz = oz + z * grid.dz();
				// うねりの移送は座標が連続になるようにローカル座標基準
				glm::vec3 g = evalGerstner(waves, waveCount, time, bx, bz);
				scratch[i] = { bx + g.x,h[i] + g.y,bz + g.z };
			}

		for (uint32_t z = 0; z < nz; ++z)
			for (uint32_t x = 0; x < nx; ++x)
			{
				const size_t i = static_cast<size_t>(z) * nx + x;
				const uint32_t xl = x > 0 ? x - 1 : x, xr = x + 1 < nx ? x + 1 : x;
				const uint32_t zd = z > 0 ? z - 1 : z, zu = z + 1 < nz ? z + 1 : z;
				const glm::vec3 dpx = scratch[static_cast<size_t>(z) * nx + xr] - scratch[static_cast<size_t>(z) * nx + xl];
				const glm::vec3 dpz = scratch[static_cast<size_t>(zu) * nx + x] - scratch[static_cast<size_t>(zd) * nx + x];

				glm::vec3 n = glm::cross(dpz, dpx);
				const float len = glm::length(n);
				n = len > 1e-8f ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
				const float tl = glm::length(dpx);
				const glm::vec3 t = tl > 1e-8f ? dpx / tl : glm::vec3(1.0f, 0.0f, 0.0f);

				V& v = out[i];
				v.position = scratch[i];
				v.color = { 1.0f,1.0f,1.0f,1.0f };
				v.uv = { uvTiling * static_cast<float>(x) / static_cast<float>(nx - 1),
						uvTiling * static_cast<float>(z) / static_cast<float>(nz - 1) };
				v.normal = n;
				v.tangent = t;
				v.bitangent = glm::cross(n, t);
			}
	}
}
