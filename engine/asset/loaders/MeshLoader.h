#pragma once
#include "asset/AssetManager.h"
#include "renderer/dx11/DX11Renderer.h"
#include <vector>
#include <glm/glm.hpp>

namespace FaluEngine {

    struct SubMesh {
        uint32_t indexOffset;
        uint32_t indexCount;
    };

struct MeshAsset : Asset {
    // assimp でロードした頂点・インデックスデータをここに格納する
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> subMeshes;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
};

std::shared_ptr<MeshAsset> loadMesh(const std::string& path,ID3D11Device* device);

void registerMeshLoader(ID3D11Device* device);

} // namespace FaluEngine
