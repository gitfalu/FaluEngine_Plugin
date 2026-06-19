#pragma once 
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include "asset/AssetManager.h"

using Microsoft::WRL::ComPtr;

namespace FaluEngine
{
	struct ShaderAsset : public Asset
	{
		ComPtr<ID3D11VertexShader> vertexShader;
		ComPtr<ID3D11PixelShader> pixelShader;
		ComPtr<ID3D11InputLayout> inputLayout;
		bool valid = false;
	};

	std::shared_ptr<ShaderAsset> loadeShader(
		const std::string& vsPath, const std::string& psPath, ID3D11Device* device);

	void registerShaderLoader(ID3D11Device* device);
}
