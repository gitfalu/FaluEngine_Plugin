#include "MaterialLoader.h"
#include "core/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>


using json = nlohmann::json;


namespace FaluEngine
{
	//==== glm convert helper =====
	static json vec3ToJson(const glm::vec3& v)
	{
		return { {"x",v.x},{"y",v.y} ,{"z",v.z} };
	}
	static json vec4ToJson(const glm::vec4& v)
	{
		return { {"x",v.x},{"y",v.y} ,{"z",v.z},{"w",v.w} };
	}
	
	static glm::vec3 vec3FromJson(const json& j, const glm::vec4& def = {})
	{
		if (!j.is_object()) return def;
		return { j.value("x",def.x),j.value("y",def.y),j.value("z",def.z) };
	}
	static glm::vec4 vec4FromJson(const json& j,const glm::vec4& def = {})
	{
		if (!j.is_object()) return def;
		return { 
			j.value("x",def.x),j.value("y",def.y),
			j.value("z",def.z),j.value("z",def.z)};
	}

	//=========== Load =================

	std::shared_ptr<MaterialAsset> loadMaterial(const std::string& path)
	{
		auto mat = std::make_shared<MaterialAsset>();

		if (!std::filesystem::exists(path))
		{
			LOG_ERROR("MaterialLoader: file not found '{}'", path);
			return mat;
		}

		std::ifstream file(path);
		if (!file.is_open()) {
			LOG_ERROR("MaterialLoader: failed to open '{}'", path);
			return mat;
		}

		json root;
		try{
			file >> root;
		}
		catch(const json::exception& e)
		{
			LOG_ERROR("MaterialLoader: JSON parse error in '{}': '{}'", path, e.what());
			return mat;
		}

		mat->albedoColor = vec4FromJson(root.value("albedoColor", json{}));

		mat->metallic				= root.value("metallic", 0.0f);
		mat->roughness				= root.value("roughness", 0.5f);
		mat->metallicMapPath		= root.value("metallicMapPath", "");
		mat->normalMapPath			= root.value("normalMapPath", "");
		mat->aoMapPath				= root.value("aoMapPath", "");
		mat->emissiveMapPath		= root.value("emissiveMapPath", "");
		mat->emissiveColor			= vec3FromJson(root.value("emissiveColor", json{}));
		mat->emissiveStrength = root.value("emissiveStrength", 1.0f);

		mat->vertexShaderPath = root.value("vertexShaderPath", "");
		mat->pixelShaderPath = root.value("pixelShaderPath", "");

		mat->valid = true;
		LOG_INFO("MaterialLoader:loaded '{}'", path);
		return mat;
	}

	bool saveMaterial(const std::string& path, const MaterialAsset& material)
	{
		json root;

		root["albedoColor"] = vec4ToJson(material.albedoColor);
		root["albedoMapPath"] = material.albedoMapPath;

		root["metallic"] = material.metallic;
		root["roughness"] = material.roughness;
		root["metallicMapPath"] = material.metallicMapPath;
		root["normalMapPath"] = material.normalMapPath;
		root["aoMapPath"] = material.aoMapPath;
		root["emissiveMapPath"] = material.emissiveMapPath;
		root["emissivColor"] = vec3ToJson(material.emissiveColor);
		root["emissiveStrength"] = material.emissiveStrength;

		root["vertexShaderPath"] = material.vertexShaderPath;
		root["pixelShaderPath"] = material.pixelShaderPath;

		std::ofstream file(path);
		if (!file.is_open()) {
			LOG_ERROR("MaterialLoader: failed to open '{}' for writing", path);
			return false;
		}

		file << root.dump(4);
		LOG_INFO("MaterialLoader: saved '{}'", path);
		return true;
	}

	void registerMaterialLoader()
	{
		AssetManager::get().registerLoader<MaterialAsset>(
			[](const std::string& path) {
				return loadMaterial(path);
			});
	}

}
