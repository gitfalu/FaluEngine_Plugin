#pragma once
#include "TerrainComponent.h"
#include <FaluEngine/EngineExport.h>
#include <d3d11.h>

namespace FaluEngine
{
	class Scene;
	class Entity;

	// TerrainComponentを受け取り地形のメッシュを生成

	FALU_ENGINE_API Entity createTerrainEntity(
		Scene& scene,
		TerrainComponent params,
		ID3D11Device* device,
		const char* name = "Terrain"
	);

	FALU_ENGINE_API void addTerrainComponents(
		Scene& scene,
		Entity entity,
		TerrainComponent params,
		ID3D11Device* device
	);

	FALU_ENGINE_API void regenerateTerrain(
		Scene& scene,
		Entity entity,
		ID3D11Device* device
	);

	/// @brief スカルプト編集用：現在の高さからメッシュのみ作り直す
	/// @param scene 
	/// @param entity 
	/// @param device 
	/// @return 
	FALU_ENGINE_API void rebuildTerrainMeshOnly(
		Scene& scene,
		Entity entity,
		ID3D11Device* device
	);

	/// @brief スカルプト編集用：現在の高さ情報から物理形状だけを作り直す
	/// @param scene 
	/// @param entity 
	/// @return 
	FALU_ENGINE_API void syncTerrainPhysics(
		Scene& scene,
		Entity entity
	);

	/// @brief シリアライズ復元用
	/// @param scene 
	/// @param device 
	/// @return 
	FALU_ENGINE_API void rebuildAllPendingTerrains(
		Scene& scene,
		ID3D11Device* device
	);

}
