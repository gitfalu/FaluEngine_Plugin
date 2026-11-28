/*****************************************************************//**
 * \file   ClothComponent.h
 * \brief  PBRの布を風で揺らすメッシュベースのコンポーネント
 * 
 * \author falu
 * \date   November 2026
 *********************************************************************/
#pragma once
#include "ClothSolver.h"
#include "RippleComponent.h"
#include <memory>
#include <vector>
#include <cstdint>
#include <FaluEngine/EngineExport.h>

namespace FaluEngine
{
	struct MeshAsset;

	struct FALU_SCENE_API ClothComponent
	{
		// 保存するデータ
		bool simulate = true; 
		uint32_t resX = 24; // 横の頂点数(2..128)
		uint32_t resY = 24; // 横の頂点数(2..128)
		float width = 2.0f;
		float height = 2.0f;
		int pinMode = 0; // ClothPinMode: 0= 上限 1=上端の画角 2=なし
		bool doubleSided = true;
		float uvTiling = 1.0f;

		int substeps = 4;
		int iterations = 6;
		float stiffness = 1.0f;
		float bendStiffness = 0.3f;
		float damping = 0.2f;
		float gravity = 9.8f;
		float airDrag = 0.25f;
		float areaDensity = 0.5f;
		float thickness = 0.02f;
		float windInfluence = 1.0f; // 風の影響倍率

		bool groundEnabed = false;
		float groundY = 0.0f;
		float friction = 0.5f;
		ClothSphere spheres[4]; // ワールド空間の球コライダー

		//=== ランタイムで使用する情報 =====
		ClothSim sim;
		std::vector<glm::vec3> scratchPos, scratchNrm;
		std::weak_ptr<MeshAsset> ownedMesh;
		float time = 0.0f;
		int warmup = 0;
		bool built = false;
		uint32_t builtResX = 0, builtResY = 0;
		float builtW = 0.0f, builtH = 0.0f;
		int builtPin = -1;
		bool builtDouble = false;
		SimStats stats;
	};
}
