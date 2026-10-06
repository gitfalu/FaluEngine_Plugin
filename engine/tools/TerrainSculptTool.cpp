#include "TerrainsculptTool.h"
#include "terrain/TerrainComponent.h"
#include "terrain/TerrainFactory.h"
#include "renderer/dx11/DX11Renderer.h"
#include"FaluEngine/Scene.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"
#include <algorithm>
#include <cmath>

namespace Editor
{
	TerrainSculptTool& TerrainSculptTool::get()
	{
		static TerrainSculptTool instance;
		return instance;
	}

	void TerrainSculptTool::setTarget(FaluEngine::Scene* scene, entt::entity entity)
	{
		m_scene = scene;
		m_entity = entity;
		m_hasHit = false;
		m_wasMouseDown = false;
	}

	void TerrainSculptTool::clearTarget()
	{
		m_scene = nullptr;
		m_entity = entt::null;
		enabled = false;
		m_hasHit = false;
		m_wasMouseDown = false;
	}

	void TerrainSculptTool::update(FaluEngine::DX11Renderer* renderer, ID3D11Device* device, float mouseX, float mouseY, float imageW, float imageH, bool mouseDown, bool invertModifierHeld, float deltaTime)
	{
		m_hasHit = false;

		if (!enabled || !renderer || !device || !hasTarget())
		{
			m_wasMouseDown = false;
			return;
		}

		if (!m_scene->registry().valid(m_entity) ||
			!m_scene->registry().all_of<FaluEngine::TerrainComponent, FaluEngine::TransformComponent>(m_entity))
		{
			m_wasMouseDown = false;
			return;
		}

		if (imageW <= 0.0f || imageH <= 0.0f ||
			mouseX < 0.0f || mouseY < 0.0f || mouseX > imageW || mouseY > imageH)
		{
			m_wasMouseDown = false;
			return;
		}

		auto& terrain = m_scene->registry().get<FaluEngine::TerrainComponent>(m_entity);
		auto& transform = m_scene->registry().get<FaluEngine::TransformComponent>(m_entity);

		float ndcX = (mouseX / imageW) * 2.0f - 1.0f;
		float ndcY = 1.0f - (mouseY / imageH) * 2.0f;

		glm::mat4 view = renderer->getView();
		glm::mat4 proj = renderer->getProjection();
		glm::mat4 invVP = glm::inverse(proj * view);

		glm::vec4 clip(ndcX, ndcY, 0.5f, 1.0f);
		glm::vec4 worldH = invVP * clip;
		if (std::fabs(worldH.w) < 1e-8f)
		{
			m_wasMouseDown = false;
			return;
		}
		worldH /= worldH.w;

		glm::vec3 rayOrigin = renderer->getCameraPosition();
		glm::vec3 rayDir = glm::normalize(glm::vec3(worldH) - rayOrigin);


		glm::vec3 hitLocal;
		float maxDist = (std::max)(terrain.sizeX, terrain.sizeZ) * 4.0f + 100.0f;
		bool hit = raycastHeightfield(terrain, transform.worldMatrix, rayOrigin, rayDir, maxDist, hitLocal);

		if (hit)
		{
			m_hasHit = true;
			m_lastHitLocal = hitLocal;
			m_lastHitWorld = glm::vec3(transform.worldMatrix * glm::vec4(hitLocal, 1.0f));

			bool strokeStart = mouseDown && !m_wasMouseDown;
			if (strokeStart && mode == BrushMode::Flatten)
				m_flattenTargetHeight = hitLocal.y;

			if (mouseDown)
			{
				applyBrush(terrain, hitLocal, deltaTime, invertModifierHeld);
				FaluEngine::rebuildTerrainMeshOnly(*m_scene, FaluEngine::Entity(m_entity, m_scene), device);
			}
		}

		bool strokeEnded = m_wasMouseDown && !mouseDown;
		if (strokeEnded)
		{
			FaluEngine::syncTerrainPhysics(*m_scene, FaluEngine::Entity(m_entity, m_scene));
		}
		m_wasMouseDown = mouseDown;
	}

	bool TerrainSculptTool::raycastHeightfield(FaluEngine::TerrainComponent& terrain, const glm::mat4& worldMatrix, const glm::vec3& rayOrigin, const glm::vec3& rayDir, float maxDistance, glm::vec3& outHitLocal) const
	{
		if (terrain.heights.empty()) return false;

		glm::mat4 invWorld = glm::inverse(worldMatrix);
		glm::vec3 localOrigin = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
		glm::vec3 localDir = glm::vec3(invWorld * glm::vec4(rayDir, 0.0f));
		float dirLenSq = glm::dot(localDir, localDir);
		if (dirLenSq < 1e-10f) return false;
		localDir = glm::normalize(localDir);

		const uint32_t vx = terrain.vertexCountX();
		const float stepX = terrain.sizeX / static_cast<float>(terrain.resolutionX);
		const float stepZ = terrain.sizeZ / static_cast<float>(terrain.resolutionZ);

		auto sampleHeight = [&](float x, float z)->float {
			float gx = glm::clamp(x / stepX, 0.0f, static_cast<float>(terrain.resolutionX));
			float gz = glm::clamp(z / stepZ, 0.0f, static_cast<float>(terrain.resolutionZ));
			int x0 = static_cast<int>(gx);
			int z0 = static_cast<int>(gz);
			int x1 = (std::min)(x0 + 1, static_cast<int>(terrain.resolutionX));
			int z1 = (std::min)(z0 + 1, static_cast<int>(terrain.resolutionZ));
			float tx = gx - static_cast<float>(x0);
			float tz = gz - static_cast<float>(z0);

			float h00 = terrain.heights[static_cast<size_t>(z0) * vx + x0];
			float h10 = terrain.heights[static_cast<size_t>(z0) * vx + x1];
			float h01 = terrain.heights[static_cast<size_t>(z1) * vx + x0];
			float h11 = terrain.heights[static_cast<size_t>(z1) * vx + x1];

			float a = h00 + (h10 - h00) * tx;
			float b = h01 + (h11 - h01) * tx;
			return a + (b - a) * tz;
			};

		float stepSize = (std::min)(stepX, stepZ) * 0.5f;
		if (stepSize <= 0.0f) return false;

		auto inBoundsXZ = [&](const glm::vec3& p)
			{
				return p.x >= 0.0f && p.x <= terrain.sizeX && p.z >= 0.0f && p.z <= terrain.sizeZ;
			};

		float t = 0.0f;
		glm::vec3 p0 = localOrigin;
		bool wasInBounds = inBoundsXZ(p0);
		float prevDiff = p0.y - sampleHeight(
			glm::clamp(p0.x,0.0f,terrain.sizeX),
			glm::clamp(p0.z,0.0f,terrain.sizeZ));

		while (t < maxDistance)
		{
			t += stepSize;
			glm::vec3 p = localOrigin + localDir * t;
			bool inBounds = inBoundsXZ(p);

			if (inBounds)
			{
				float diff = p.y - sampleHeight(p.x, p.z);
				if (wasInBounds && diff <= 0.0f && prevDiff > 0.0f)
				{
					float tLo = t - stepSize, tHi = t;
					for (int i = 0; i < 10; ++i)
					{
						float tm = (tLo + tHi) * 0.5f;
						glm::vec3 pm = localOrigin + localDir * tm;
						float dm = pm.y - sampleHeight(pm.x, pm.z);
						if (dm > 0.0f) tLo = tm; else tHi = tm;
					}

					outHitLocal = localOrigin + localDir * ((tLo + tHi) * 0.5f);
					return true;
				}

				prevDiff = diff;
			}
			wasInBounds = inBounds;
		}

		return false;
	}

	void TerrainSculptTool::applyBrush(FaluEngine::TerrainComponent& terrain, const glm::vec3& hitLocal, float dt, bool invert)
	{
		const uint32_t vx = terrain.vertexCountX();
		const float stepX = terrain.sizeX / static_cast<float>(terrain.resolutionX);
		const float stepZ = terrain.sizeZ / static_cast<float>(terrain.resolutionZ);

		int cx = static_cast<int>(hitLocal.x / stepX);
		int cz = static_cast<int>(hitLocal.z / stepZ);
		int rx = static_cast<int>(std::ceil(radius / stepX)) + 1;
		int rz = static_cast<int>(std::ceil(radius / stepZ)) + 1;

		int xMin = (std::max)(0, cx - rx);
		int xMax = (std::min)(static_cast<int>(terrain.resolutionX), cx + rx);
		int zMin = (std::max)(0, cz - rz);
		int zMax = (std::min)(static_cast<int>(terrain.resolutionZ), cz + rz);

		bool effectiveLower = (mode == BrushMode::Lower);
		if (invert && (mode == BrushMode::Raise || mode == BrushMode::Lower))
			effectiveLower = !effectiveLower;

		for (int z = zMin; z <= zMax; ++z)
		{
			for (int x = xMin; x <= xMax; ++x)
			{
				float wx = static_cast<float>(x) * stepX;
				float wz = static_cast<float>(z) * stepZ;
				float dist = std::sqrt((wx - hitLocal.x) * (wx - hitLocal.x) + (wz - hitLocal.z) * (wz - hitLocal.z));
				if (dist > radius) continue;

				float t = dist / radius;
				float falloff = 0.5f * (std::cos(t * 3.14159265f) + 1.0f);

				size_t idx = static_cast<size_t>(z) * vx + x;
				float& h = terrain.heights[idx];

				switch (mode)
				{
				case Editor::BrushMode::Raise:
				case Editor::BrushMode::Lower:
					h += (effectiveLower ? -1.0f : 1.0f) * strength * falloff * dt;
					break;
				case Editor::BrushMode::Flatten:
					h = h + (m_flattenTargetHeight - h) * glm::clamp(strength * falloff * dt, 0.0f, 1.0f);
					break;
				case Editor::BrushMode::Smooth:
					float sum = 0.0f; int count = 0;
					for (int nz = -1; nz <= 1; ++nz)
					{
						for (int nx = -1; nx <= 1; ++nx)
						{
							int sx = (std::max)(0, (std::min)(static_cast<int>(terrain.resolutionX), x + nx));
							int sz = (std::max)(0, (std::min)(static_cast<int>(terrain.resolutionZ), z + nz));
							sum += terrain.heights[static_cast<size_t>(sz) * vx + sx];
							++count;
						}
					}
					float avg = sum / static_cast<float>(count);
					h = h + (avg - h) * glm::clamp(strength * 0.3f * falloff * dt, 0.0f, 1.0f);

					break;
				}
			}
		}
	}

	
}
