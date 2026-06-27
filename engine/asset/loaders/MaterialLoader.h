#pragma once
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include "asset/AssetManager.h"
#include "asset/loaders/TextureLoader.h"
#include "asset/loaders/ShaderLoader.h"

namespace FaluEngine
{
	//=== Material Asset ====
	struct MaterialAsset: public Asset
	{
		glm::vec4 albedoColor = { 1.0f,1.0f,1.0f,1.0f };
		std::string albedoMapPath;

		//=== PBR ====
		float metallic = 0.0f;
		float roughness = 0.5f;
		std::string metallicMapPath;
		std::string normalMapPath;
		std::string aoMapPath;
		std::string emissiveMapPath;
		glm::vec3 emissiveColor = { 0.0f,0.0f,0.0f };
		float emissiveStrength = 1.0f;

		std::string vertexShaderPath;
		std::string pixelShaderPath;

		std::shared_ptr<TextureAsset> cachedAlbedoMap;
		std::shared_ptr<TextureAsset> cachedMetallicMap;
		std::shared_ptr<TextureAsset> cachedNormalMap;
		std::shared_ptr<TextureAsset> cachedAOMap;
		std::shared_ptr<TextureAsset> cachedEmissiveMap;
		std::shared_ptr<ShaderAsset> cachedShader;

		bool valid = false;
	};

	std::shared_ptr<MaterialAsset> loadMaterial(const std::string& path);

	bool saveMaterial(const std::string& path, const MaterialAsset& material);

	void registerMaterialLoader();


}
