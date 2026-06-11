#include "SceneSerializer.h"
#include "Scene.h"
#include "Entity.h"
#include "Component.h"
#include "physics/RigidbodyComponent.h"
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
	static json quatToJson(const glm::quat& q)
	{
		return { {"x",q.x},{"y",q.y} ,{"z",q.z},{"w",q.w} };
	}

	static glm::vec3 vec3FromJson(const json& j)
	{
		return { j["x"],j["y"],j["z"] };
	}
	static glm::quat quatFromJson(const json& j)
	{
		return { 
			j["w"].get<float>(),j["x"].get<float>(),
			j["y"].get<float>(),j["z"].get<float>() };
	}
	
	bool SceneSerializr::serialize(const std::string& path) const
	{
		json root;
		root["scene"] = m_scene.getName();
		root["version"] = "1.0";
		root["entities"] = json::array();

		std::unordered_map<entt::entity, uint32_t> entityToId;
		uint32_t idCounter = 0;

		auto view = m_scene.registry().view<TagComponent>();
		for (auto entity : view)
			entityToId[entity] = idCounter++;

		for (auto entity : view)
		{
			json entityJson;
			auto& tag = view.get<TagComponent>(entity);
			entityJson["name"] = tag.name;
			entityJson["id"] = entityToId[entity];

			if (m_scene.registry().all_of<TransformComponent>(entity))
			{
				auto& t = m_scene.registry().get<TransformComponent>(entity);
				entityJson["transform"] = {
					{"position",vec3ToJson(t.position)},
					{"rotation",quatToJson(t.rotation)},
					{"scale",vec3ToJson(t.scale)}
				};
			}

			if (m_scene.registry().all_of<MeshComponent>(entity))
			{
				auto& m = m_scene.registry().get<MeshComponent>(entity);
				entityJson["mesh"] = {
					{"meshPath",m.meshPath},
					{"texturePath",m.texturePath},
					{"visible",m.visible}
				};
			}

			if (m_scene.registry().all_of<CameraComponent>(entity))
			{
				auto& cam = m_scene.registry().get<CameraComponent>(entity);
				entityJson["camera"] = {
					{"fovDeg",cam.camera.getFovDeg()},
					{"nearClip",cam.camera.getNearClip()},
					{"farClip",cam.camera.getFarClip()},
					{"isPrimary",cam.isPrimary},
					{"position",vec3ToJson(cam.camera.getPosition())},
					{"yaw",cam.camera.getYaw()},
					{"pitch",cam.camera.getPitch()}
				};
			}

			if (m_scene.registry().all_of<RigidbodyComponent>(entity))
			{
				auto& rb = m_scene.registry().get<RigidbodyComponent>(entity);
				entityJson["rigidbody"] = {
					{"bodyType",static_cast<int>(rb.bodyType)},
					{"shape",static_cast<int>(rb.shape)},
					{"halfExtents",vec3ToJson(rb.halfExtents)},
					{"radius",rb.radius},
					{"height",rb.height},
					{"mass",rb.mass},
					{"restitution",rb.restitution},
					{"friction",rb.friction},
					{"useGravity",rb.useGravity}
				};
			}

			if (m_scene.registry().all_of<ScriptComponent>(entity))
			{
				auto& sc = m_scene.registry().get<ScriptComponent>(entity);
				entityJson["script"] = {
					{"scriptPath",sc.scriptPath}
				};
			}

			// RelationshipComponent
			if (m_scene.registry().all_of<RelationshipComponent>(entity))
			{
				auto& rel = m_scene.registry().get<RelationshipComponent>(entity);
				if (rel.parent != entt::null && entityToId.count(rel.parent))
				{
					entityJson["parent"] = entityToId[rel.parent];
				}
			}

			root["entities"].push_back(entityJson);
		}

		std::filesystem::create_directories(
			std::filesystem::path(path).parent_path());

		std::ofstream file(path);
		if (!file.is_open()) {
			LOG_ERROR("SceneSerializer: failed to open '{}' for writting",path);
			return false;
		}

		file << root.dump(4);
		LOG_INFO("SceneSerializer: saved '{}' ({} entities)", path, view.size());

		return true;
	}
	bool SceneSerializr::deserialize(const std::string& path)
	{
		if (!std::filesystem::exists(path))
		{
			LOG_ERROR("SceneSerializer: file not found '{}'", path);
			return false;
		}
		
		std::ifstream file(path);
		if (!file.is_open()) {
			LOG_ERROR("SceneSerializer: failed to open '{}'", path);
			return false;
		}

		json root;
		try {
			file >> root;
		}
		catch (const json::exception& e) {
			LOG_ERROR("SceneSerializer: JSON perse error: {}", e.what());
			return false;
		}

		auto existing = m_scene.registry().view<TagComponent>();
		for (auto entity : existing)
		{
			Entity e(entity, &m_scene);
			m_scene.destroyEntity(e);
		}

		std::unordered_map<uint32_t, entt::entity> idMap;

		// restoration
		for (const auto& entityJson : root["entities"]) {
			std::string name = entityJson.value("name", "Entity");
			Entity entity = m_scene.createEntity(name);
			uint32_t savedId = entityJson.value("id", 0u);
			idMap[savedId] = static_cast<entt::entity>(entity);

			// TransformComponent
			if (entityJson.contains("transform")) {
				auto& t = entity.getComponent<TransformComponent>();
				auto& tj = entityJson["transform"];
				t.position = vec3FromJson(tj["position"]);
				t.rotation = quatFromJson(tj["rotation"]);
				t.scale = vec3FromJson(tj["scale"]);
			}

			// MeshComponent
			if (entityJson.contains("mesh")) {
				auto& mj = entityJson["mesh"];
				auto& mesh = entity.addComponent<MeshComponent>();
				mesh.meshPath = mj.value("meshPath", "");
				mesh.texturePath = mj.value("texturePath", "");
				mesh.visible = mj.value("visible", true);
			}

			// CameraComponent
			if (entityJson.contains("camera")) {
				auto& cj = entityJson["camera"];
				auto& cam = entity.addComponent<CameraComponent>();
				cam.isPrimary = cj.value("isPrimary", false);
				cam.camera.setPerspective(
					cj.value("fovDeg", 60.0f),
					cam.camera.getAspectRatio(),
					cj.value("nearClip", 0.1f),
					cj.value("farClip", 1000.0f));
				if (cj.contains("position")) {
					cam.camera.setPosition(vec3FromJson(cj["position"]));
					cam.camera.setYaw(cj.value("yaw", 90.0f));
					cam.camera.setPitch(cj.value("pitch", 0.0f));
				}
			}

			// RigidbodyComponent
			if (entityJson.contains("rigidbody")) {
				auto& rj = entityJson["rigidbody"];
				auto& rb = entity.addComponent<RigidbodyComponent>();
				rb.bodyType = static_cast<BodyType>(rj.value("bodyType",1));
				rb.shape = static_cast<ColliderShape>(rj.value("shape", 0));
				rb.halfExtents = vec3FromJson(rj["halfExtents"]);
				rb.radius = rj.value("radius", 0.5f);
				rb.height = rj.value("height", 1.0f);
				rb.mass= rj.value("mass", 1.0f);
				rb.restitution = rj.value("restitution", 0.3f);
				rb.friction= rj.value("friction", 1.0f);
				rb.useGravity= rj.value("useGravity", true);
			}

			// ScriptComponent
			if (entityJson.contains("script")) {
				auto& sj = entityJson["script"];
				auto& sc = entity.addComponent<ScriptComponent>();
				sc.scriptPath = sj.value("scriptPath", "");
			}

			
		}

		// eŽqŠÖŒW‚Ì•œŒ³
		for (const auto& entityJson : root["entities"])
		{
			if (!entityJson.contains("parent")) continue;

			uint32_t savedId = entityJson.value("id", 0u);
			uint32_t savedParentId = entityJson.value("parent", 0u);

			auto itChild = idMap.find(savedId);
			auto itParent = idMap.find(savedParentId);
			if (itChild == idMap.end() || itParent == idMap.end()) continue;

			Entity child(itChild->second, &m_scene);
			Entity parent(itParent->second, &m_scene);
			child.setParent(parent);
		}


		LOG_INFO("SceneSerializer: loaded '{}' ({} entities)", 
			path, root["entities"].size());
		return true;
	}
}
