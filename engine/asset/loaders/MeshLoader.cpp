#include "MeshLoader.h"
#include "core/Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>

namespace FaluEngine {

std::shared_ptr<MeshAsset> loadMesh(const std::string& path, ID3D11Device* device) {
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("MeshLoader: file not found '{}'", path);
        return nullptr;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace |
        aiProcess_GenNormals
        );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("MeshLoader: assimp error - {}", importer.GetErrorString());
        return nullptr;
    }

    auto asset = std::make_shared<MeshAsset>();

    for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];

        SubMesh sub;
        sub.indexOffset = static_cast<uint32_t>(asset->indices.size());
        sub.indexCount = mesh->mNumFaces * 3;

        uint32_t vertexBase = static_cast<uint32_t>(asset->vertices.size());

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
            Vertex vert;
            vert.position = {
                mesh->mVertices[v].x,
                mesh->mVertices[v].y,
                mesh->mVertices[v].z
            };
            //-Color
            if (mesh->HasVertexColors(0)) {
                vert.color = {
                    mesh->mColors[0][v].r,
                    mesh->mColors[0][v].g,
                    mesh->mColors[0][v].b,
                    mesh->mColors[0][v].a
                };
            }
            else
            {
                vert.color = { 0.9f,0.9f,0.9f,1.0f };
            }

            //- UV
            if (mesh->HasTextureCoords(0))
            {
                vert.uv = {
                    mesh->mTextureCoords[0][v].x,
                    mesh->mTextureCoords[0][v].y
                };
            }
            else
            {
                vert.uv = { 0.0f,0.0f };
            }

            // normal
            if (mesh->HasNormals()) {
                vert.normal = {
                    mesh->mNormals[v].x,
                    mesh->mNormals[v].y,
                    mesh->mNormals[v].z
                };
            }
            else
            {
                vert.normal = { 0.0f,1.0f,0.0f };
            }

            // tangent / bitangent
            if (mesh->HasTangentsAndBitangents()) {
                vert.tangent = {
                    mesh->mTangents[v].x,
                    mesh->mTangents[v].y,
                    mesh->mTangents[v].z
                };
                vert.bitangent = {
                    mesh->mBitangents[v].x,
                    mesh->mBitangents[v].y,
                    mesh->mBitangents[v].z
                };
            }
            else
            {
                vert.tangent = { 1.0f,0.0f,0.0f };
                vert.bitangent = { 0.0f,0.0f,1.0f };
            }

            asset->vertices.push_back(vert);
        }

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f) 
        {
            const aiFace& face = mesh->mFaces[f];
            for (uint32_t i = 0; i < face.mNumIndices; ++i)
            {
                asset->indices.push_back(vertexBase + face.mIndices[i]);
            }
        }
        
        asset->subMeshes.push_back(sub);
    }
    
    if (device) {
        D3D11_BUFFER_DESC vbd = {};
        vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * asset->vertices.size());
        vbd.Usage = D3D11_USAGE_IMMUTABLE;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vData = {};
        vData.pSysMem = asset->vertices.data();
        device->CreateBuffer(&vbd, &vData, &asset->vertexBuffer);

        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * asset->indices.size());
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA iData = {};
        iData.pSysMem = asset->indices.data();
        device->CreateBuffer(&ibd, &iData, &asset->indexBuffer);
    }

    LOG_INFO("MeshLoader: loaded '{}' ({} vertices, {} indices)",
        path, asset->vertices.size(), asset->indices.size());

    return asset;
}

void registerMeshLoader(ID3D11Device* device)
{
    AssetManager::get().registerLoader<MeshAsset>(
        [device](const std::string& path) {
            return loadMesh(path, device);
        }
    );
}

} // namespace FaluEngine
