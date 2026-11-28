#include "ClothSolver.h"
#include <cmath>

namespace FaluEngine
{
	void ClothSim::build(uint32_t nx, uint32_t ny, float width, float height, ClothPinMode pin, const glm::mat4& world)
	{
		m_nx = (std::max)(nx, 2u);
		m_ny = (std::max)(ny, 2u);
		const size_t n = static_cast<size_t>(m_nx) * m_ny;
		const float dx = width / static_cast<float>(m_nx - 1);
		const float dy = height / static_cast<float>(m_ny - 1);

		m_restLocal.resize(n);
		m_invMass.assign(n, 1.0f);
		for(uint32_t j = 0;j < m_ny;++j)
			for (uint32_t i = 0; i < m_nx; ++i)
			{
				const size_t k = static_cast<size_t>(j) * m_nx + i;
				m_restLocal[k] = { i * dx - 0.5f * width,-static_cast<float>(j) * dy ,0.0f };
				if (j == 0)
				{
					if (pin == ClothPinMode::TopEdge) m_invMass[k] = 0.0f;
					else if (pin == ClothPinMode::TopCorner && (i == 0 || i == m_nx - 1))
						m_invMass[k] = 0.0f;
				}
			}

		m_edges.clear();
		auto add = [&](uint32_t a, uint32_t b, bool bend)
			{
				m_edges.push_back({ a,b,glm::length(m_restLocal[a] - m_restLocal[b]),bend });
			};
		auto id = [&](uint32_t i, uint32_t j) { return j * m_nx + i; };
		for (uint32_t j = 0; j < m_ny; ++j)
			for (uint32_t i = 0; i < m_nx; ++i)
			{
				if (i + 1 < m_nx) add(id(i, j), id(i + 1, j), false);
				if (j + 1 < m_ny) add(id(i, j), id(i, j + 1), false);
				if (i + 1 < m_nx && j + 1 < m_ny)
				{
					add(id(i, j), id(i + 1, j + 1), false);
					add(id(i + 1, j), id(i, j + 1), false);
				}
			}

		m_cellArea = (std::max)(width * height / static_cast<float>(n), 1e-6f);
		m_tris = buildClothIndices(m_nx, m_ny, false);
		m_vel.assign(n, glm::vec3(0.0f));
		m_force.assign(n, glm::vec3(0.0f));
		m_contact.assign(n, 0);
		m_pos.resize(n); m_prev.resize(n); m_pinPrev.resize(n); m_pinTarget.resize(n);
		reset(world);
	}

	void ClothSim::updatePins(const glm::mat4& world)
	{
		for (size_t k = 0; k < m_pos.size(); ++k)
		{
			if (m_invMass[k] != 0.0f) continue;
			m_pinPrev[k] = m_pinTarget[k];
			m_pinTarget[k] = glm::vec3(world * glm::vec4(m_restLocal[k], 1.0f));
		}
	}

	void ClothSim::step(float dt, const ClothParams& params, const WindParams& wind, float time, const ClothSphere* spheres, int sphereCount)
	{
		if (m_pos.empty() || dt <= 0.0f) return;

		const int sub = std::clamp(params.substeps, 1, 16);
		const int iters = std::clamp(params.iterations, 1, 32);
		const float h = dt / static_cast<float>(sub);
		const size_t n = m_pos.size();
		const glm::vec3 g(0.0f, -params.gravity, 0.0f);
		const float dampK = std::exp(-(std::max)(params.damping, 0.0f) * h);
		const bool hasWind = wind.speed > 0.0f;
		const float invParticleMass = 1.0f / (std::max)(params.areaDensity * m_cellArea, 1e-6f);

		for (int s = 0; s < sub; ++s)
		{
			const float alpha = static_cast<float>(s + 1) / static_cast<float>(sub);
			const float tNow = time + h * static_cast<float>(s);

			//===== 外力：風/空気抵抗 =======
			std::fill(m_force.begin(), m_force.end(), glm::vec3(0.0f));
			if (params.airDrag > 0.0f)
			{
				for (size_t t = 0; t + 2 < m_force.size(); t += 3)
				{
					const uint32_t a = m_tris[t], b = m_tris[t + 1], c = m_tris[t + 2];
					const glm::vec3 cen = (m_pos[a] + m_pos[b] + m_pos[c]) * (1.0f / 3.0f);
					const glm::vec3 vel = (m_vel[a] + m_vel[b] + m_vel[c]) * (1.0f / 3.0f);
					const glm::vec3 rel = (hasWind ? evalWind(wind, cen, tNow) : glm::vec3(0.0f)) - vel;
					glm::vec3 nrm = glm::cross(m_pos[b] - m_pos[a], m_pos[c] - m_pos[a]);
					const float area2 = glm::length(nrm);
					if (area2 < 1e-10f) continue;
					nrm /= area2;
					// 面に垂直な成分のみ受ける　圧力 = airDeg*|vn|*vn 、力 = 圧力 * 面積
					const float vn = glm::dot(nrm, rel);
					const glm::vec3 f = nrm * (params.airDrag * std::abs(vn) * vn * 0.5f * area2);
					m_force[a] += f * (1.0f / 3.0f);
					m_force[b] += f * (1.0f / 3.0f);
					m_force[c] += f * (1.0f / 3.0f);
				}
			}

			//==== 予測 ======
			for (size_t i = 0; i < n; ++i)
			{
				if (m_invMass[i] == 0.0f)
				{
					m_prev[i] = m_pos[i];
					m_pos[i] = glm::mix(m_pinPrev[i], m_pinTarget[i], alpha);
					m_vel[i] = glm::vec3(0.0f);
					continue;
				}
				glm::vec3 aero = m_force[i] * invParticleMass;
				const float al = glm::length(aero);
				if (al > 500.0f) aero *= 500.0f / al;
				glm::vec3 v = m_vel[i] + (g + aero) * h;
				v *= dampK;
				const float sp = glm::length(v);
				if (sp > params.maxSpeed) v *= params.maxSpeed / sp;
				m_prev[i] = m_pos[i];
				m_pos[i] += v * h;
				m_contact[i] = 0;
			}

			//==== 拘束 =====
			for (int it = 0; it < iters; ++it)
			{
				for (const Edge& e : m_edges)
				{
					const float wa = m_invMass[e.a], wb = m_invMass[e.b];
					const float w = wa + wb;
					if (w == 0.0f) continue;
					glm::vec3 d = m_pos[e.b] - m_pos[e.a];
					const float len = glm::length(d);
					if (len < 1e-8f) continue;
					const float k = e.bend ? params.bendStiffness : params.stiffness;
					const glm::vec3 corr = d * (k * (len - e.rest) / (len * w));
					m_pos[e.a] += corr * wa;
					m_pos[e.b] -= corr * wb;
				}
			}

			//==== 衝突 ========
			for (size_t i = 0; i < n; ++i)
			{
				if (m_invMass[i] == 0.0f) continue;
				glm::vec3& x = m_pos[i];
				for (int si = 0; si < sphereCount; ++si)
				{
					if (spheres[si].radius <= 0.0f) continue;
					const float r = spheres[si].radius + params.thickness;
					const glm::vec3 d = x - spheres[si].center;
					const float l2 = glm::dot(d, d);
					if (l2 < r * r)
					{
						const float l = std::sqrt(l2);
						x = spheres[si].center + (l > 1e-8f ? d / l : glm::vec3(0.0f, 1.0f, 0.0f)) * r;
						m_contact[i] = 1;
					}
				}

				if (params.groundEnabled && x.y < params.groundY + params.thickness)
				{
					x.y = params.groundY + params.thickness;
					m_contact[i] = 1;
				}
			}

			//=============== 速度更新 ==============
			const float fr = std::clamp(params.friction, 0.0f, 1.0f);
			for (size_t i = 0; i < n; ++i)
			{
				if (m_invMass[i] == 0.0f) continue;
				m_vel[i] == (m_pos[i] - m_prev[i]) / h;
				m_vel[i] == (m_pos[i] - m_prev[i]) / h;
				if (m_contact[i]) m_vel[i] *= (1.0f - 0.5f * fr);
			}
		}

	}

	void ClothSim::reset(const glm::mat4& world)
	{
		for (size_t k = 0; k < m_pos.size(); ++k)
		{
			m_pos[k] = m_prev[k] = m_pinPrev[k] = m_pinTarget[k] =
				glm::vec3(world * glm::vec4(m_restLocal[k], 1.0f));
			m_vel[k] = glm::vec3(0.0f);
		}
	}

	float ClothSim::maxStretch() const
	{
		float m = 0.0f;
		for (const auto& e : m_edges)
		{
			if (e.bend || e.rest < 1e-8f) continue;
			m = (std::max)(m, glm::length(m_pos[e.a] - m_pos[e.b]) / e.rest - 1.0f);
		}

		return m;
	}

	std::vector<uint32_t> buildClothIndices(uint32_t nx, uint32_t ny, bool doubleSided)
	{
		std::vector<uint32_t> idx;
		if (nx < 2 || ny < 2) return idx;
		const uint32_t n = nx * ny;
		idx.reserve(static_cast<size_t>(nx - 1) * (ny - 1) * 6 * (doubleSided ? 2 : 1));
		auto quad = [&](uint32_t off, bool flip, uint32_t i, uint32_t j)
			{
				const uint32_t i0 = off + j * nx + i, i1 = i0 + 1, i2 = i0 + nx, i3 = i2 + 1;
				// 前面は -Z向き
				if (!flip)
				{
					idx.push_back(i0); idx.push_back(i1); idx.push_back(i2);
					idx.push_back(i1); idx.push_back(i3); idx.push_back(i2);
				}
				else
				{
					idx.push_back(i0); idx.push_back(i2); idx.push_back(i1);
					idx.push_back(i1); idx.push_back(i2); idx.push_back(i3);
				}
			};
		for(uint32_t j = 0;j + 1 < ny;++j)
			for (uint32_t i = 0; i + 1 < nx; ++i)
			{
				quad(0, false, i, j);
				if (doubleSided) quad(n, true, i, j);
			}
		return idx;
	}
}