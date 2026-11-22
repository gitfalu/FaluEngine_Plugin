#include "DynamicMesh.h"
#include "core/Logger.h"
#include <string>

namespace FaluEngine
{
	std::shared_ptr<MeshAsset> createDynamicMeshAsset(
		ID3D11Device* device, std::vector<Vertex>&& vertices, 
		std::vector<uint32_t>&& indices)
	{
		auto asset = std::make_shared<MeshAsset>();
		asset->vertices = std::move(vertices);
		asset->indices = std::move(indices);

		SubMesh sub{};
		sub.indexOffset = 0;
		sub.indexCount = static_cast<uint32_t>(asset->indices.size());
		asset->subMeshes.push_back(sub);

		if (!device) return asset;

		D3D11_BUFFER_DESC vbd = {};
		vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * asset->vertices.size());
		vbd.Usage = D3D11_USAGE_DYNAMIC;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		D3D11_SUBRESOURCE_DATA vData = {};
		vData.pSysMem = asset->vertices.data();
		if (FAILED(device->CreateBuffer(&vbd, &vData, &asset->vertexBuffer)))
			LOG_ERROR("DynamicMesh: CreateBuffer(vertex) failed");

		D3D11_BUFFER_DESC ibd = {};
		ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * asset->indices.size());
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA iData = {};
		iData.pSysMem = asset->indices.data();
		if (FAILED(device->CreateBuffer(&ibd, &iData, &asset->indexBuffer)))
			LOG_ERROR("DynamicMesh: CreateBuffer(index) failed");

		return asset;
	}
	
	bool uploadDynamicVertices(ID3D11DeviceContext* context, MeshAsset& asset)
	{
		if (!context || !asset.vertexBuffer || asset.vertices.empty()) return false;

		// 頂点バッファの実サイズと一致しない場合は書き込まない(バッファ溢れ防止)
		D3D11_BUFFER_DESC desc{};
		asset.vertexBuffer->GetDesc(&desc);
		if (desc.Usage != D3D11_USAGE_DYNAMIC ||
			desc.ByteWidth != sizeof(Vertex) * asset.vertices.size())
		{
			LOG_ERROR("DynamicMesh: vertex count/buffer size mismatch (buffer {} bytes, cpu {} bytes)",
				desc.ByteWidth, sizeof(Vertex) * asset.vertices.size());
			return false;
		}

		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(asset.vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return false;
		std::memcpy(mapped.pData, asset.vertices.data(), sizeof(Vertex) * asset.vertices.size());
		context->Unmap(asset.vertexBuffer.Get(), 0);
		return true;
	}
}
