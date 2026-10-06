#pragma once 
#include <vector>
#include <cstdint>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	// 地形コンポーネント
	// MeshComponentとRigidbodyComponentを組として１エンティティに付与
	struct FALU_ENGINE_API TerrainComponent
	{
		//-グリッドの分割数
		uint32_t resolutionX = 128;
		uint32_t resolutionZ = 128;

		//-ワールド空間でのサイズ
		float sizeX = 100.0f;
		float sizeZ = 100.0f;

		//-高さのスケール(出力にかける値)
		float heightScale = 20.0f;

		//-fBmノイズパラメータ
		float noiseFrequency = 0.02f;
		int octaves = 4;
		float lacunarity = 2.0f; // 空間の隙間
		float persistence = 0.5f;
		uint32_t seed = 1337;

		//-生成された高さデータ
		std::vector<float> heights;

		bool dirty = true;

		[[nodiscard]] uint32_t vertexCountX() const noexcept { return resolutionX + 1; }

		[[nodiscard]] uint32_t vertexCountZ() const noexcept { return resolutionZ + 1; }
	};
}
