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
		/// @param s シード値
		/// @return 
		float nextRand01(uint32_t& s)
		{
			if (s == 0)s = 0x9E3779B9u;
			s ^= s << 13; s ^= s >> 17; s ^= s << 5;
			return static_cast<float>(s & 0xFFFFFF) / static_cast<float>(0x1000000);
		}

		/// @brief 境界値によって制限し無害化
		/// @param r 水面コンポーネント
		void sanitize(RippleComponent& r)
		{
			r.resolution = std::clamp(r.resolution, 2u, 512u);
			r.sizeX = (std::max)(r.sizeX, 0.01f);
			r.sizeZ = (std::max)(r.sizeZ, 0.01f);
			r.waveSpeed = (std::max)(r.waveSpeed, 0.0f);
			r.damping = (std::max)(r.damping, 0.0f);
			r.maxHeight = (std::max)(r.maxHeight, 0.001f);
		}

		/// @brief 波情報の更新を確実に行うための関数
		/// @param r 
		/// @param mesh 
		/// @param device 
		void ensureBuilt(RippleComponent& r, MeshComponent& mesh, ID3D11Device* device)
		{
			sanitize(r);
			// cachedMesh が自分で作ったものか(meshPath経由で別メッシュが入っていないか)も確認する
			// 頂点数の違うメッシュへ書き込むとバッファ溢れ(ヒープ破壊)になるため
			const size_t expectVerts = static_cast<size_t>(r.resolution) * r.resolution;
			const bool ownsMesh = mesh.cachedMesh &&
				r.ownedMesh.lock() == mesh.cachedMesh &&
				mesh.cachedMesh->vertices.size() == expectVerts;
			// 変更が加わっていたら波情報を更新するようにする
			const bool upToDate = r.built && ownsMesh &&
				r.builtResolution == r.resolution &&
				r.builtSizeX == r.sizeX && r.builtSizeZ == r.sizeZ;
			if (upToDate) return;
			// 波の情報を初期化する
			r.grid.resize(r.resolution, r.resolution, r.sizeX, r.sizeZ);
			r.pending.clear();
			r.dropAccum = 0.0f;
			if (r.rng == 0) r.rng = r.seed ? r.seed : 1u;
			// 波の頂点情報をビルドする
			std::vector<Vertex> verts;
			buildRippleVertices<Vertex>(r.grid, r.waves, 4, r.time * r.waveTimeScale,
				r.centered, r.uvTiling, r.scratch, verts);
			// グリッドのインデックス情報もビルド
			auto idx = buildGridIndices(r.resolution, r.resolution);
			// 動的メッシュを作成しメッシュをキャッシュ化する
			mesh.cachedMesh = createDynamicMeshAsset(device, std::move(verts), std::move(idx));
			mesh.visible = true;
			r.ownedMesh = mesh.cachedMesh;
			// 波のコンポーネントを初期状態にする
			r.built = true;
			r.builtResolution = r.resolution;
			r.builtSizeX = r.sizeX;
			r.builtSizeZ = r.sizeZ;
			r.stats.vertexCount = r.resolution * r.resolution;
		}
	} // Helper


	
	/// @brief 水面更新関数(秘匿)
	/// @param scene 
	/// @param dt 
	static void updateRipples(Scene& scene, float dt)
	{
		// レジストリから水面とメッシュのコンポーネントを取得
		auto& reg = scene.registry();
		auto view = reg.view<RippleComponent, MeshComponent>();
		if (view.begin() == view.end()) return;
		// レンダラーを取得してデバイスとコンテキストを取得
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
						// 雨滴位置を範囲から計算
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
			// 
			const float total = r.stats.stepMs + r.stats.meshMs;
			r.stats.avgTotalMs = r.stats.avgTotalMs == 0.0f ? total
				: r.stats.avgTotalMs + (total - r.stats.avgTotalMs) * 0.05f;
		}
	}

	void addRippleComponents(Scene& scene, Entity entity, RippleComponent params)
	{
		(void)scene;
		if (entity.hasComponent<RippleComponent>())
		{// コンポーネントが二重に付くことを防ぐ
			LOG_WARN("addRippleComponents: entity already has a RippleComponent.");
			return;
		}
		// 水面コンポーネントをパラメータを適用して付与
		entity.addComponent<RippleComponent>(std::move(params));
		// メッシュコンポーネントがない場合は付与する
		if (!entity.hasComponent<MeshComponent>())
			entity.addComponent<MeshComponent>();
	}

	void addRippleImpulse(Scene& scene, entt::entity entity, const glm::vec3& worldPos, float strength, float radius)
	{
		auto& reg = scene.registry();
		if (!reg.valid(entity) || !reg.all_of<RippleComponent, TransformComponent>(entity)) return;
		// 波をワールドからローカルの座標に変換
		auto& r = reg.get<RippleComponent>(entity);
		const glm::mat4 inv = glm::inverse(reg.get<TransformComponent>(entity).worldMatrix);
		const glm::vec3 local = glm::vec3(inv * glm::vec4(worldPos, 1.0f));
		// ローカルの座標として保存
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

	/*
	
	風

	*/

	namespace
	{
		WindParams g_wind; // 今のフレームのシーン風
		float g_simTime = 0.0f; // 全シミュレーション共通の時間
	}

	const WindParams& getSceneWind()
	{	
		return g_wind;
	}

	glm::vec3 sampleSceneWind(const glm::vec3& world)
	{
		return evalWind(g_wind, world, g_simTime);
	}

	/// @brief 風更新関数(秘匿)
	/// @param scene 
	/// @param dt 
	static void updateWind(Scene& scene, float dt)
	{
		g_simTime += (std::min)(dt, 1.0f / 30.0f);
		g_wind = WindParams{};
		g_wind.speed = 0.0f;
		auto view = scene.registry().view<WindComponent>();
		for (auto e : view)
		{
			const auto& w = view.get<WindComponent>(e);
			if (!w.enabled) continue;
			g_wind = w.params;
			break; // 最初の有効な風を使用するためループを終了する
		}
	}

	/*
	
	布

	*/

	namespace
	{
		void sanitize(ClothComponent& c)
		{
			c.resX = std::clamp(c.resX, 2u, 128u);
			c.resY = std::clamp(c.resY, 2u, 128u);
			c.width = (std::max)(c.width, 0.01f);
			c.height = (std::max)(c.height, 0.01f);
			c.pinMode = std::clamp(c.pinMode, 0, 2);
			c.substeps = std::clamp(c.substeps, 1, 16);
			c.iterations = std::clamp(c.iterations, 1, 32);
			c.areaDensity = (std::max)(c.areaDensity, 0.001f);
		}

		void ensureClothBuilt(ClothComponent& c, MeshComponent& mesh, ID3D11Device* device,
			const glm::mat4& world)
		{
			sanitize(c);
			const size_t n = static_cast<size_t>(c.resX) * c.resY;
			const size_t expectVerts = c.doubleSided ? n * 2 : n;
			const bool ownsMesh = mesh.cachedMesh && c.ownedMesh.lock() == mesh.cachedMesh &&
				mesh.cachedMesh->vertices.size() == expectVerts;
			const bool upToDate = c.built && ownsMesh &&
				c.builtResX == c.resX && c.builtResY == c.resY &&
				c.builtW == c.width && c.builtH == c.height &&
				c.builtPin == c.pinMode && c.builtDouble == c.doubleSided;
			if (upToDate) return;

			c.sim.build(c.resX, c.resY, c.width, c.height, static_cast<ClothPinMode>(c.pinMode), world);
			const glm::mat4 inv = glm::inverse(world);

			std::vector<Vertex> verts;
			buildClothVertices<Vertex>(c.sim, inv, c.uvTiling, c.doubleSided, c.scratchPos, c.scratchNrm, verts);
			auto idx = buildClothIndices(c.resX, c.resY, c.doubleSided);

			mesh.cachedMesh = createDynamicMeshAsset(device, std::move(verts), std::move(idx));
			mesh.visible = true;
			c.ownedMesh = mesh.cachedMesh;

			c.built = true;
			c.builtResX = c.resX; c.builtResY = c.resY;
			c.builtW = c.width; c.builtH = c.height;
			c.builtPin = c.pinMode; c.builtDouble = c.doubleSided;
			c.warmup = 2; // ワールド行列は前フレーム値なので最初のフレームは再配置する
			c.stats.vertexCount = static_cast<uint32_t>(expectVerts);
		}
	}

	void addClothComponents(Scene& scene, Entity entity, ClothComponent params)
	{
		(void)scene;
		if (entity.hasComponent<ClothComponent>())
		{
			LOG_WARN("addClothComponents: entity already has a ClothComponent.");
			return;
		}
		entity.addComponent<ClothComponent>(std::move(params));
		if (!entity.hasComponent<MeshComponent>())
			entity.addComponent<MeshComponent>();
	}

	void resetCloth(Scene& scene, entt::entity entity)
	{
		auto& reg = scene.registry();
		if (!reg.valid(entity) || !reg.all_of<ClothComponent, TransformComponent>(entity)) return;

		auto& c = reg.get<ClothComponent>(entity);
		if (c.built) c.sim.reset(reg.get<TransformComponent>(entity).worldMatrix);
		c.time = 0.0f;
	}
	 
	static void updateCloths(Scene& scene, float dt)
	{
		auto& reg = scene.registry();
		auto view = reg.view<ClothComponent, MeshComponent, TransformComponent>();
		if (view.begin() == view.end()) return;

		auto* renderer = static_cast<DX11Renderer*>(getActiveRenderer());
		if (!renderer) return;
		ID3D11Device* device = renderer->getDevice();
		ID3D11DeviceContext* ctx = renderer->getContext();
		const float cdt = (std::min)(dt, 1.0f / 30.0f);

		for (auto e : view)
		{
			auto& c = view.get<ClothComponent>(e);
			auto& mesh = view.get<MeshComponent>(e);
			const glm::mat4& world = view.get<TransformComponent>(e).worldMatrix;

			ensureClothBuilt(c, mesh, device, world);
			if (!mesh.cachedMesh) continue;

			const auto tStep = Clock::now();
			if (c.warmup > 0) { c.sim.reset(world); --c.warmup; }
			if (c.simulate)
			{
				ClothParams p;
				p.substeps = c.substeps; p.iterations = c.iterations;
				p.stiffness = c.stiffness; p.bendStiffness = c.bendStiffness;
				p.damping = c.damping; p.gravity = c.gravity;
				p.airDrag = c.airDrag; p.areaDensity = c.areaDensity;
				p.thickness = c.thickness; p.groundEnabled = c.groundEnabed;
				p.groundY = c.groundY; p.friction = c.friction;

				WindParams wind = g_wind;
				wind.speed *= c.windInfluence;

				c.sim.updatePins(world);
				c.sim.step(cdt, p, wind, g_simTime, c.spheres, 4);
				c.time += cdt;
			}
			c.stats.stepMs = msSince(tStep);

			const auto tMesh = Clock::now();
			buildClothVertices<Vertex>(c.sim, glm::inverse(world), c.uvTiling, c.doubleSided,
				c.scratchPos, c.scratchNrm, mesh.cachedMesh->vertices);
			uploadDynamicVertices(ctx, *mesh.cachedMesh);
			c.stats.meshMs = msSince(tMesh);

			const float total = c.stats.stepMs + c.stats.meshMs;
			c.stats.avgTotalMs = c.stats.avgTotalMs == 0.0f ? total
				: c.stats.avgTotalMs + (total - c.stats.avgTotalMs) * 0.05f;

		}
	}

	void FaluEngine::updateSimulation(Scene& scene, float dt)
	{
		updateWind(scene, dt);
		updateRipples(scene, dt);
		updateCloths(scene, dt);
	}
}
