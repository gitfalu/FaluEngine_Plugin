#include "ShaderLoader.h"
#include "renderer/dx11/DX11Renderer.h"
#include "core/Logger.h"
#include <d3dcompiler.h>
#include <filesystem>
#include "core/PathResolver.h"

namespace FaluEngine
{
	std::shared_ptr<ShaderAsset> loadeShader(const std::string& vsPath, const std::string& psPath, ID3D11Device* device)
	{
		auto asset = std::make_shared<ShaderAsset>();
		
		if (!std::filesystem::exists(vsPath))
		{
			LOG_ERROR("ShaderLoader: VS not found '{}'", vsPath);
			return asset;
		}

		if (!std::filesystem::exists(psPath))
		{
			LOG_ERROR("ShaderLoader: PS not found '{}'", psPath);
			return asset;
		}

		UINT compileFlags = 0;
#ifdef ENGINE_DEBUG
		compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

		// Compile Vertex Shader
		HRESULT hr = D3DCompileFromFile(
			std::wstring(vsPath.begin(), vsPath.end()).c_str(),
			nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0,
			&vsBlob, &errBlob);
		if (FAILED(hr))
		{
			if (errBlob)
				LOG_ERROR("ShaderLoader: VS compile error '{}': {}",
					vsPath, static_cast<char*>(errBlob->GetBufferPointer()));
			return asset;
		}

		// Compile Pixel Shader
		hr = D3DCompileFromFile(
			std::wstring(psPath.begin(), psPath.end()).c_str(),
			nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0,
			&psBlob, &errBlob);
		if (FAILED(hr))
		{
			if (errBlob)
				LOG_ERROR("ShaderLoader: PS compile error '{}': {}",
					psPath, static_cast<char*>(errBlob->GetBufferPointer()));
			return asset;
		}

		device->CreateVertexShader(
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			nullptr, &asset->vertexShader);
		device->CreatePixelShader(
			psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			nullptr, &asset->pixelShader);

		D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
		offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,
		offsetof(Vertex,color),D3D11_INPUT_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
		offsetof(Vertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
		offsetof(Vertex,normal),D3D11_INPUT_PER_VERTEX_DATA},
		{"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
		offsetof(Vertex,tangent),D3D11_INPUT_PER_VERTEX_DATA,0},
		{"BINORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
		offsetof(Vertex,bitangent),D3D11_INPUT_PER_VERTEX_DATA,0},
		};

		hr = device->CreateInputLayout(
			layout, ARRAYSIZE(layout),
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			&asset->inputLayout);
		if (FAILED(hr))
		{
			LOG_ERROR("ShaderLoader: CreateInputLayout failed for '{}'", vsPath);
			return asset;
		}

		asset->valid = true;
		LOG_INFO("ShaderLoader: loaded '{}' / '{}'", vsPath, psPath);
		return asset;
	}

	void registerShaderLoader(ID3D11Device* device)
	{
		AssetManager::get().registerLoader<ShaderAsset>(
			[device](const std::string& key) {
				auto sep = key.find('|');
				if (sep == std::string::npos)return std::shared_ptr<ShaderAsset>();
				std::string vsPath = key.substr(0, sep);
				std::string psPath = key.substr(sep + 1);
				return loadeShader(vsPath, psPath, device);
			});
	}
}

