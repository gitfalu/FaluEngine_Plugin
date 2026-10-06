#pragma once
#include "TerrainComponent.h"
#include "asset/loaders/MeshLoader.h"
#include <FaluEngine/EngineExport.h>
#include <memory>
#include <d3d11.h>

namespace FaluEngine
{
	FALU_ENGINE_API void generateTerrainHeights(TerrainComponent& terrain);
	
	FALU_ENGINE_API std::shared_ptr<MeshAsset> buildTerrainMesh(TerrainComponent& terrain, ID3D11Device* device);
}
