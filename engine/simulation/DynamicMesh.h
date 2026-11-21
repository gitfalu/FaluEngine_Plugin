/*****************************************************************//**
 * \file   DynamicMesh.h
 * \brief  CPUで舞フレーム頂点を書き換えるメッシュ
 * 
 * \author tsunn
 * \date   November 2026
 *********************************************************************/
#pragma once
#include "asset/loaders/MeshLoader.h"
#include "renderer/dx11/DX11Renderer.h"
#include <memory>
#include <vector>
#include <d3d11.h>
#include <FaluEngine/EngineExport.h>


namespace FaluEngine
{
	/// @brief 動的頂点編集メッシュを作成
	/// @param device 
	/// @param vertices 
	/// @param indices 
	/// @return 
	FALU_SCENE_API std::shared_ptr<MeshAsset> createDynamicMeshAsset(
		ID3D11Device* device,
		std::vector<Vertex>&& vertices,
		std::vector<uint32_t>&& indices);

	/// @brief GPUへ頂点を転送する
	/// @param context 
	/// @param asset 
	/// @return 
	FALU_SCENE_API bool uploadDynamicVertices(ID3D11DeviceContext* context, MeshAsset& asset);
}

