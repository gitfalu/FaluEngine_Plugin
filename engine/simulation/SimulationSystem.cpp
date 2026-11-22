#include "SimulationSystem.h"
#include "DynamicMesh.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"
#include "renderer/IRenderer.h"
#include "renderer/dx11/DX11Renderer.h"
#include "core/Logger.h"
#include <chrono>
#include <cmath>

namespace FaluEngine
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		float msSince(Clock::time_point t0)
		{
			return std::chrono::duration<float, std::milli>(Clock::now() - t0).count();
		}

		/// @brief xorshift32: 同じシードで同じ雨滴列(再現性保持)
		/// @param s 
		/// @return 
		float nextRand01(uint32_t& s)
		{
			if (s == 0)s = 0x9E3779B9u;
			s ^= s << 13; s ^= s >> 17; s ^= s << 5;
			return static_cast<float>(s & 0xFFFFFF) / static_cast<float>(0x1000000);
		}

		void sanitize(RippleComponent& r)
		{
			r.resolution = std::clamp(r.resolution, 2u, 512u);
			r.sizeX = (std::max)(r.sizeX, 0.01f);
			r.sizeZ = (std::max)(r.sizeZ, 0.01f);
			r.waveSpeed = (std::max)(r.waveSpeed, 0.0f);
			r.damping = (std::max)(r.damping, 0.0f);
			r.maxHeight = (std::max)(r.maxHeight, 0.001f);
		}

		void ensureBuilt(RippleComponent& r, MeshComponent& mesh, ID3D11Device* device)
		{
			sanitize(r);
			// cachedMesh が自分で作ったものか(meshPath経由で別メッシュが入っていないか)も確認する
			// 頂点数の違うメッシュへ書き込むとバッファ溢れ(ヒープ破壊)になるため
			const size_t expectVerts = static_cast<size_t>(r.resolution) * r.resolution;
			const bool ownsMesh = mesh.cachedMesh &&
				r.ownedMesh.lock() == mesh.cachedMesh &&
				mesh.cachedMesh->vertices.size() == expectVerts;

			const bool upToDate = r.built && ownsMesh &&
				r.builtResolution == r.resolution &&
				r.builtSizeX == r.sizeX && r.builtSizeZ == r.sizeZ;
			if (upToDate) return;

			r.grid.resize(r.resolution, r.resolution, r.sizeX, r.sizeZ);
			r.pending.clear();
			r.dropAccum = 0.0f;
			if (r.rng == 0) r.rng = r.seed ? r.seed : 1u;

			std::vector<Vertex> verts;
			buildRippleVertices<Vertex>(r.grid, r.waves, 4, r.time * r.waveTimeScale,
				r.centered, r.uvTiling, r.scratch, verts);
			auto idx = buildGridIndices(r.resolution, r.resolution);

			mesh.cachedMesh = createDynamicMeshAsset(device, std::move(verts), std::move(idx));
			mesh.visible = true;
			r.ownedMesh = mesh.cachedMesh;

			r.built = true;
			r.builtResolution = r.resolution;
			r.builtSizeX = r.sizeX;
			r.builtSizeZ = r.sizeZ;
			r.stats.vertexCount = r.resolution * r.resolution;
		}
	} // Helper


	
	void updateSimulation(Scene& scene, float dt)
	{
		auto& reg = scene.registry();
		auto view = reg.view<RippleComponent, MeshComponent>();
		if (view.begin() == view.end()) return;

		auto* renderer = static_cast<DX11Renderer*>(getActiveRenderer());
		if (!renderer) return; // 終了処理中などのレンダラーがない時はＧＰＵリソースは触らない
		ID3D11Device* device = renderer->getDevice();
		ID3D11DeviceContext* ctx = renderer->getContext();

		const float cdt = (std::min)(dt, 1.0f / 30.0f); // スパイク時に波が暴れないよう上限をつける

		for (auto e : view)
		{
			auto& r = view.get<RippleComponent>(e);
			auto& mesh = view.get<MeshComponent>(e);

			ensureBuilt(r, mesh, device);
			if (!mesh.cachedMesh) continue;

			const auto tStep = Clock::now();
			if (r.simulate)
			{
				// 自動雨滴
				if (r.dropPerSecond > 0.0f)
				{
					r.dropAccum += r.dropPerSecond * cdt;
					while (r.dropAccum >= 1.0f)
					{
						r.dropAccum -= 1.0f;
						const float margin = r.dropRadius;
						const float x = margin + nextRand01(r.rng) * (std::max)(r.sizeX - 2.0f * margin, 0.0f);
						const float z = margin + nextRand01(r.rng) * (std::max)(r.sizeZ - 2.0f * margin, 0.0f);
						r.pending.push_back({ x,z,-r.dropStrength,r.dropRadius });
					}
				}
				for (const auto& imp : r.pending)
					r.grid.addImpulse(imp.x, imp.z, imp.strength, imp.radius, r.maxHeight);
				r.pending.clear();

				RippleSolverParams sp;
				sp.waveSpeed = r.waveSpeed;
				sp.damping = r.damping;
				sp.maxHeight = r.maxHeight;
				sp.absorbWidth = r.absorbWidth;
				r.grid.step(cdt, sp);
				r.time += cdt;
			}
			r.stats.stepMs = msSince(tStep);
			r.stats.subSteps = r.grid.lastSubSteps();

			// 頂点生成->GPUアップロード
			const auto tMesh = Clock::now();
			buildRippleVertices<Vertex>(r.grid, r.waves, 4, r.time * r.waveTimeScale,
				r.centered, r.uvTiling, r.scratch, mesh.cachedMesh->vertices);
			uploadDynamicVertices(ctx, *mesh.cachedMesh);
			r.stats.meshMs = msSince(tMesh);

			const float total = r.stats.stepMs + r.stats.meshMs;
			r.stats.avgTotalMs = r.stats.avgTotalMs == 0.0f ? total
				: r.stats.avgTotalMs + (total - r.stats.avgTotalMs) * 0.05f;
		}
	}

	void addRippleComponents(Scene& scene, Entity entity, RippleComponent params)
	{
		(void)scene;
		if (entity.hasComponent<RippleComponent>())
		{
			LOG_WARN("addRippleComponents: entity already has a RippleComponent.");
			return;
		}
		entity.addComponent<RippleComponent>(std::move(params));
		if (!entity.hasComponent<MeshComponent>())
			entity.addComponent<MeshComponent>();
	}

	void addRippleImpulse(Scene& scene, entt::entity entity, const glm::vec3& worldPos, float strength, float radius)
	{
		auto& reg = scene.registry();
		if (!reg.valid(entity) || !reg.all_of<RippleComponent, TransformComponent>(entity)) return;

		auto& r = reg.get<RippleComponent>(entity);
		const glm::mat4 inv = glm::inverse(reg.get<TransformComponent>(entity).worldMatrix);
		const glm::vec3 local = glm::vec3(inv * glm::vec4(worldPos, 1.0f));

		const float lx = local.x + (r.centered ? 0.5f * r.sizeX : 0.0f);
		const float lz = local.z + (r.centered ? 0.5f * r.sizeZ : 0.0f);
		r.pending.push_back({ lx,lz,strength,radius });
	}

	void resetRipple(RippleComponent& ripple)
	{
		ripple.grid.clear();
		ripple.pending.clear();
		ripple.time = 0.0f;
		ripple.dropAccum = 0.0f;
		ripple.rng = ripple.seed ? ripple.seed : 1u;
	}

}
