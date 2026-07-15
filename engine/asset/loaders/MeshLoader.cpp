#include "MeshLoader.h"
#include "core/Logger.h"
#include "renderer/dx11/DX11Renderer.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

namespace FaluEngine {

    static glm::mat4 aiMatToGlm(const aiMatrix4x4& m)
    {
        return glm::mat4(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4
        );
    }

    static void extractBoneWeights(
        const aiMesh* mesh, Skeleton& skeleton,
        std::vector<BoneWeightData>& boneWeightData
    )
    {
        for (uint32_t b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            std::string boneName = bone->mName.C_Str();

            int boneIndex = skeleton.findBoneIndex(boneName);
            if (boneIndex < 0)
            {
                Bone newBone;
                newBone.name = boneName;
                newBone.offsetMatrix = aiMatToGlm(bone->mOffsetMatrix);
                boneIndex = static_cast<int>(skeleton.bones.size());
                skeleton.boneNameToIndex[boneName] = boneIndex;
                skeleton.bones.push_back(newBone);
            }

            for (uint32_t w = 0; w < bone->mNumWeights; ++w)
            {
                uint32_t vertexId = bone->mWeights[w].mVertexId;
                float weight = bone->mWeights[w].mWeight;
                if (vertexId < boneWeightData.size())
                    boneWeightData[vertexId].addBonedata(boneIndex, weight);
            }
        }
    }

    static void buildBoneHierarchy(
        const aiNode* node, Skeleton& skeleton, int parentIndex,
        std::vector<BoneChainLink> pendingChain = {}
    )
    {
        std::string nodeName = node->mName.C_Str();
        glm::mat4 nodeTransform = aiMatToGlm(node->mTransformation);

        pendingChain.push_back({ nodeName,nodeTransform });

        int boneIndex = skeleton.findBoneIndex(nodeName);

        int currentParent = parentIndex;
        std::vector<BoneChainLink> nextChain = pendingChain;

        if (boneIndex >= 0)
        {
            skeleton.bones[boneIndex].parentIndex = parentIndex;
            skeleton.bones[boneIndex].chain =
                pendingChain;
            glm::mat4 bindTransform = glm::mat4(1.0f);
            for (const auto& link : pendingChain)
                bindTransform = bindTransform * link.staticTransform;
            skeleton.bones[boneIndex].localBindTransform = bindTransform;

            currentParent = boneIndex;
            nextChain.clear();
        }

        for (uint32_t i = 0; i < node->mNumChildren; ++i)
        {
            buildBoneHierarchy(node->mChildren[i], skeleton, currentParent,nextChain);
        }
    }

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
        //|
        //aiProcess_LimitBoneWeights // １頂点当たりのボーンのリミット
        );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR("MeshLoader: assimp error - {}", importer.GetErrorString());
        return nullptr;
    }

    auto asset = std::make_shared<MeshAsset>();
    
    //====== ボーンの有無のチェック
    bool anyBones = false;
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        if (scene->mMeshes[m]->mNumBones > 0) { anyBones = true; break; }
    }
    asset->hasSkeleton = anyBones;
    asset->globalInverseTransform =
        glm::inverse(aiMatToGlm(scene->mRootNode->mTransformation));

    //====== メッシュの読み込み
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m) {
        const aiMesh* mesh = scene->mMeshes[m];

        SubMesh sub;
        sub.indexOffset = static_cast<uint32_t>(asset->indices.size());
        sub.indexCount = mesh->mNumFaces * 3;

        uint32_t vertexBase = anyBones
            ? static_cast<uint32_t>(asset->skinnedVertices.size())
            : static_cast<uint32_t>(asset->vertices.size());

        // ボーンのWeightの一時バッファ
        std::vector<BoneWeightData> boneWeightData(mesh->mNumVertices);
        if (anyBones)
            extractBoneWeights(mesh, asset->skeleton, boneWeightData);

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
            glm::vec3 position = {
                mesh->mVertices[v].x,
                mesh->mVertices[v].y,
                mesh->mVertices[v].z};
            //-Color
            glm::vec4 color = mesh->HasVertexColors(0)
                ? glm::vec4(mesh->mColors[0][v].r,
                            mesh->mColors[0][v].g,
                            mesh->mColors[0][v].b,
                            mesh->mColors[0][v].a)
            : glm::vec4( 0.9f,0.9f,0.9f,1.0f );
            //- UV
            glm::vec2 uv = mesh->HasTextureCoords(0)
                ? glm::vec2(mesh->mTextureCoords[0][v].x,
                            mesh->mTextureCoords[0][v].y)
                :glm::vec2( 0.0f,0.0f );
            // normal
            glm::vec3 normal = mesh->HasNormals()
                ? glm::vec3(mesh->mNormals[v].x,
                            mesh->mNormals[v].y,
                            mesh->mNormals[v].z)
                :glm::vec3( 0.0f, 1.0f, 0.0f );
            // tangent / bitangent
            glm::vec3 tangent = { 1.0f,0.0f,0.0f };
            glm::vec3 bitangent = { 0.0f,0.0f,1.0f };
            if (mesh->HasTangentsAndBitangents()) {
                tangent = {
                    mesh->mTangents[v].x,
                    mesh->mTangents[v].y,
                    mesh->mTangents[v].z
                };
                bitangent = {
                    mesh->mBitangents[v].x,
                    mesh->mBitangents[v].y,
                    mesh->mBitangents[v].z
                };
            }
            if (anyBones)
            {
                SkinnedVertex sv;
                sv.position = position;
                sv.color = color;
                sv.uv = uv;
                sv.normal = normal;
                sv.tangent = tangent;
                sv.bitangent = bitangent;

                boneWeightData[v].normalize();
                sv.boneIndices = {
                    boneWeightData[v].boneIndices[0],boneWeightData[v].boneIndices[1],
                    boneWeightData[v].boneIndices[2],boneWeightData[v].boneIndices[3]
                };
                sv.boneWeights = {
                    boneWeightData[v].boneWeights[0],boneWeightData[v].boneWeights[1],
                    boneWeightData[v].boneWeights[2],boneWeightData[v].boneWeights[3]
                };

                asset->skinnedVertices.push_back(sv);
            }
            else
            {
                Vertex vert;
                vert.position = position;
                vert.color = color;
                vert.uv = uv;
                vert.normal = normal;
                vert.tangent = tangent;
                vert.bitangent = bitangent;
                asset->vertices.push_back(vert);
            }
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
    // ボーン階層をノードツリーから構築
    if (anyBones)
    {
        buildBoneHierarchy(scene->mRootNode, asset->skeleton, -1);
    }
    
    if (device) {
        if (anyBones)
        {
            D3D11_BUFFER_DESC vbd = {};
            vbd.ByteWidth = static_cast<UINT>(sizeof(SkinnedVertex) * asset->skinnedVertices.size());
            vbd.Usage = D3D11_USAGE_IMMUTABLE;
            vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA vData = {};
            vData.pSysMem = asset->skinnedVertices.data();
            device->CreateBuffer(&vbd, &vData, &asset->vertexBuffer);
        }
        else
        {
            D3D11_BUFFER_DESC vbd = {};
            vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * asset->vertices.size());
            vbd.Usage = D3D11_USAGE_IMMUTABLE;
            vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA vData = {};
            vData.pSysMem = asset->vertices.data();
            device->CreateBuffer(&vbd, &vData, &asset->vertexBuffer);
        }

        D3D11_BUFFER_DESC ibd = {};
        ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * asset->indices.size());
        ibd.Usage = D3D11_USAGE_IMMUTABLE;
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA iData = {};
        iData.pSysMem = asset->indices.data();
        device->CreateBuffer(&ibd, &iData, &asset->indexBuffer);
    }

    LOG_INFO("MeshLoader: loaded '{}' ({} vertices, {} indices,{} bones)",
        path, 
        anyBones ? 
        asset->skinnedVertices.size() : asset->vertices.size(), 
        asset->indices.size(),
        asset->skeleton.bones.size()
    );

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
