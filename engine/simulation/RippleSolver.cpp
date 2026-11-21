#include "RippleSolver.h"

namespace FaluEngine
{
	void RippleGrid::resize(uint32_t nx, uint32_t nz, float sizeX, float sizeZ)
	{
		m_nx = (std::max)(nx, 2u);
		m_nz = (std::max)(nz, 2u);
		m_sizeX = (std::max)(sizeX, 0.001f);
		m_sizeZ = (std::max)(sizeZ, 0.001f);
		m_dx = m_sizeX / static_cast<float>(m_nx - 1);
		m_dz = m_sizeZ / static_cast<float>(m_nz - 1);
		m_h.assign(static_cast<size_t>(m_nx) * m_nz, 0.0f);
		m_v.assign(m_h.size(), 0.0f);
	}

	void RippleGrid::clear()
	{
		std::fill(m_h.begin(), m_h.end(), 0.0f);
		std::fill(m_v.begin(), m_v.end(), 0.0f);
	}

	void FaluEngine::RippleGrid::step(float dt, const RippleSolverParams& p)
	{
		if (m_h.empty() || dt <= 0.0f) { m_lastSubSteps = 0; return; }

		const float c = (std::max)(p.waveSpeed, 0.0f);
		const float minD = (std::min)(m_dx, m_dz);
		// CFL：c * dt * sqrt(1/dx^2+1/dz^2) <= -0.64f
		const float maxStep = c > 1e-4f ? 0.45f * minD / c : dt;
		const int sub = std::clamp(static_cast<int>(std::ceil(dt / maxStep)), 1, 16);
		const float sdt = dt / static_cast<float>(sub);
		m_lastSubSteps = sub;

		const float c2 = c * c;
		const float invDx2 = 1.0f / (m_dx * m_dx);
		const float invDz2 = 1.0f / (m_dz * m_dz);
		const int nx = static_cast<int>(m_nx), nz = static_cast<int>(m_nz);
		const int aw = (std::max)(p.absorbWidth, 0);

		for (int s = 0; s < sub; ++s)
		{
			for (int z = 1; z < nz - 1; ++z)
			{
				const size_t row = static_cast<size_t>(z) * nx;
				for (int x = 1; x < nx - 1; ++x)
				{
					const size_t i = row + x;
					const float h = m_h[i];
					const float lap = (m_h[i - 1] + m_h[i + 1] - 2.0f * h) * invDx2 +
						(m_h[i - nx] + m_h[i + nx] - 2.0f * h) * invDx2;
					m_v[i] += c2 * lap * sdt;
				}
			}

			for (int z = 0; z < nz; ++z)
			{
				const size_t row = static_cast<size_t>(z) * nx;
				const int dz0 = (std::min)(z, nz - 1 - z);
				for (int x = 0; x < nx; ++x)
				{
					const size_t i = row + x;
					const bool border = (x == 0 || z == 0 || x == nx - 1 || z == nz - 1);
					if (border) { m_h[i] = 0.0f; m_v[i] = 0.0f; continue; }

					// 端に近いほど減衰を強める
					float damp = p.damping;
					if(aw > 0)
					{
						const int edge = (std::min)(dz0, (std::min)(x, nx - 1 - x));
						if (edge < aw)
						{
							const float t = 1.0f - static_cast<float>(edge) / static_cast<float>(aw);
							damp += 20.0f * t * t;
						}
					}

					const float k = (std::max)(0.0f, 1.0f - damp * sdt);
					m_v[i] *= k;
					m_h[i] = std::clamp(m_h[i] + m_v[i] * sdt, -p.maxHeight, p.maxHeight);
				}
			}
		}
	}

	void RippleGrid::addImpulse(float localX, float localZ, float strength, float radius, float maxHieght)
	{
		if (m_h.empty() || radius <= 0.0f) return;
		constexpr float kPi = 3.14159265358979f;

		const int x0 = (std::max)(0, static_cast<int>(std::floor((localX - radius) / m_dx)));
		const int x1 = (std::min)(static_cast<int>(m_nx) - 1, static_cast<int>(std::ceil((localX + radius) / m_dx)));
		const int z0 = (std::max)(0, static_cast<int>(std::floor((localZ - radius) / m_dz)));
		const int z1 = (std::min)(static_cast<int>(m_nz) - 1, static_cast<int>(std::ceil((localZ + radius) / m_dz)));

		for(int z = z0;z <= z1;++z)
			for (int x = x0; x <= x1; ++x)
			{
				const float ddx = x * m_dx - localX, ddz = z * m_dz - localZ;
				const float d = std::sqrt(ddx * ddx + ddz * ddz);
				if (d >= radius) continue;
				const float w = 0.5f * (1.0f + std::cos(kPi * d / radius));
				float& h = m_h[static_cast<size_t>(z) * m_nx + x];
				h = std::clamp(h + strength * w, -maxHieght, maxHieght);
			}
	}
}
