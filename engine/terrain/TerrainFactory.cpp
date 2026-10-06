#include "TerrainFactory.h"
#include "TerrainGenerator.h"
#include "core/Logger.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"
#include "physics/RigidbodyComponent.h"
#include "physics/PhysicsSystem.h"

namespace FaluEngine
{
	namespace
	{
		void syncRigidbodyToTerrain(RigidbodyComponent& rb, TerrainComponent& terrain)
		{
			rb.bodyType = BodyType::Static;
			rb.shape = ColliderShape::HeightField;
			rb.heightFieldData = terrain.heights.data();
			rb.heightFieldSamples = terrain.vertexCountX();

			float stepX = terrain.sizeX / static_cast<float>(terrain.resolutionX);
			float stepZ = terrain.sizeZ / static_cast<float>(terrain.resolutionZ);

			rb.heightFieldScale = { stepX,1.0f,stepZ };
			rb.heightFieldOffset = { 0.0f,0.0f,0.0f };
		}

		void enforceSquareGrid(TerrainComponent& params)
		{
			if (params.resolutionX != params.resolutionZ)
			{
				LOG_WARN("Terrain: Jolt HeightFieldShape requires a square grid."
					"resolutionX({}) != resolutionZ({}); using the smaller value for both.",
					params.resolutionX, params.resolutionZ
				);
				uint32_t r = (std::min)(params.resolutionX, params.resolutionZ);
				params.resolutionX = r;
				params.resolutionZ = r;
			}
		}
	}


	Entity createTerrainEntity(Scene& scene,TerrainComponent params,
		ID3D11Device* device,const char* name)
	{
		Entity entity = scene.createEntity(name);
		addTerrainComponents(scene,entity, std::move(params), device);

		auto& terrain = entity.getComponent<TerrainComponent>();
		LOG_INFO("createTerrainEntity: '{}' created ({} x {} samples,size {} x {})",
			name, terrain.vertexCountX(), terrain.vertexCountZ(), terrain.sizeX, terrain.sizeZ);

		return entity;
	}
	
	void addTerrainComponents(Scene& scene, Entity entity, TerrainComponent params, ID3D11Device* device)
	{
		if (entity.hasComponent<TerrainComponent>())
		{
			LOG_WARN("addTerrainComponents: entity already has a TerrainComponent. Skinned.");
			return;
		}

		enforceSquareGrid(params);

		auto& terrain = entity.addComponent<TerrainComponent>(std::move(params));
		auto meshAsset = buildTerrainMesh(terrain, device);

		auto& meshComp = entity.hasComponent<MeshComponent>()
			? entity.getComponent<MeshComponent>()
			: entity.addComponent<MeshComponent>();
		meshComp.meshPath.clear();

		meshComp.cachedMesh = meshAsset;
		meshComp.visible = true;

		auto& rb = entity.hasComponent<RigidbodyComponent>()
			? entity.getComponent<RigidbodyComponent>()
			: entity.addComponent<RigidbodyComponent>();
		syncRigidbodyToTerrain(rb, terrain);
		rb.friction = 0.9f;
		rb.restitution = 0.0f;
		rb.useGravity = false;

		PhysicsSystem::get().registerScene(scene);
	}
	void regenerateTerrain(Scene& scene, Entity entity, ID3D11Device* device)
	{
		if (!entity.hasComponent<TerrainComponent>())
		{
			LOG_WARN("regenerateTerrain: entity has no TerrainComponent.");
			return;
		}

		auto& terrain = entity.getComponent<TerrainComponent>();

		if (terrain.resolutionX != terrain.resolutionZ)
		{
			uint32_t r = (std::min)(terrain.resolutionX, terrain.resolutionZ);

			terrain.resolutionX = r;
			terrain.resolutionZ = r;
		}

		generateTerrainHeights(terrain);

		rebuildTerrainMeshOnly(scene, entity, device);
		if (entity.hasComponent<RigidbodyComponent>())
		{
			syncTerrainPhysics(scene, entity);
		}
	}
	
	void rebuildTerrainMeshOnly(Scene& scene, Entity entity, ID3D11Device* device)
	{
		if (!entity.hasComponent<TerrainComponent>() || !entity.hasComponent<MeshComponent>())
		{
			LOG_WARN("rebuildTerrainMeshOnly: entity is missing TerrainComponent/MeshComponent");
			return;
		}

		auto& terrain = entity.getComponent<TerrainComponent>();
		auto meshAsset = buildTerrainMesh(terrain, device);
		
		auto& meshComp = entity.getComponent<MeshComponent>();
		meshComp.cachedMesh = meshAsset;

		(void)scene;
	}
	
	void syncTerrainPhysics(Scene& scene, Entity entity)
	{
		if (!entity.hasComponent<TerrainComponent>() || !entity.hasComponent<RigidbodyComponent>())
		{
			LOG_WARN("syncTerrainPhysics: entity is missing TerrainComponent/RigidbodyComponent");
			return;
		}

		auto& terrain = entity.getComponent<TerrainComponent>();
		auto& rb = entity.getComponent<RigidbodyComponent>();

		if (rb.registered)
		{
			PhysicsSystem::get().unregisterEntity(entity);
		}

		syncRigidbodyToTerrain(rb, terrain);

		PhysicsSystem::get().registerScene(scene);
		return void();
	}
	
	void rebuildAllPendingTerrains(Scene& scene, ID3D11Device* device)
	{
		if (!device) return;

		auto view = scene.registry().view<TerrainComponent, MeshComponent>();
		for (auto e : view)
		{
			auto& mesh = view.get<MeshComponent>(e);
			if (mesh.cachedMesh) continue;

			Entity entity(e, &scene);
			rebuildTerrainMeshOnly(scene, entity, device);
			if (entity.hasComponent<RigidbodyComponent>())
			{
				syncTerrainPhysics(scene, entity);
			}
		}
	}
}
