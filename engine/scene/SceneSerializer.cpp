#include "SceneSerializer.h"
#include "Scene.h"
#include "Entity.h"
#include "Component.h"
#include "ui/UITypes.h"
#include "physics/RigidbodyComponent.h"
#include "core/Logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace FaluEngine
{
	//==== glm convert helper =====
	static json vec2ToJson(const glm::vec2& v)
	{
		return { {"x",v.x},{"y",v.y} };
	}
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

	static glm::vec2 vec2FromJson(const json& j, const glm::vec2& def = {})
	{
		if (!j.is_object()) return def;
		return { j["x"],j["y"] };
	}
	static glm::vec3 vec3FromJson(const json& j, const glm::vec3& def = {})
	{
		if (!j.is_object()) return def;
		return { j["x"],j["y"],j["z"] };
	}

	static glm::vec4 vec4FromJson(const json& j, const glm::vec4& def = {})
	{
		if (!j.is_object()) return def;
		return { j["x"],j["y"],j["z"],j["w"]};
	}
	static glm::quat quatFromJson(const json& j, const glm::quat& def = {})
	{
		if (!j.is_object()) return def;
		return { 
			j["w"].get<float>(),j["x"].get<float>(),
			j["y"].get<float>(),j["z"].get<float>() };
	}
	
	bool SceneSerializer::serialize(const std::string& path) const
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
			entityJson["category"] = tag.category;
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
					{"materialPath",m.materialPath},
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

			// AnimatorComponent
			if (m_scene.registry().all_of<AnimatorComponent>(entity))
			{
				auto& anim = m_scene.registry().get<AnimatorComponent>(entity);
				entityJson["animator"] = {
					{"currentClipName",anim.currentClipName},
					{"playbackSpeed",anim.playbackSpeed},
					{"playing",anim.playing},
					{"loop",anim.loop},
				};
			}

			//===== UI =======
			// Canvas
			if (m_scene.registry().all_of<CanvasComponent>(entity))
			{
				auto& canvas = m_scene.registry().get<CanvasComponent>(entity);
				entityJson["canvas"] = {
					{"renderMode",static_cast<int>(canvas.renderMode)},
					{"referenceResolution",vec2ToJson(canvas.referenceResolution)},
					{"sortOtder",canvas.sortOrder},
					{"enabled",canvas.enabled}
				};
			}

			// RectTransform
			if (m_scene.registry().all_of<RectTransformComponent>(entity))
			{
				auto& rt = m_scene.registry().get<RectTransformComponent>(entity);
				entityJson["rectTransform"] = {
					{"anchorMin",vec2ToJson(rt.anchorMin)},
					{"anchorMax",vec2ToJson(rt.anchorMax)},
					{"anchoredPos",vec2ToJson(rt.anchoredPos)},
					{"sizeDelta",vec2ToJson(rt.sizeDelta)},
					{"pivot",vec2ToJson(rt.pivot)},
					{"rotation",rt.rotation},
					{"scale",vec2ToJson(rt.scale)}
				};
			}

			// Image
			if (m_scene.registry().all_of<ImageComponent>(entity))
			{
				auto& img = m_scene.registry().get<ImageComponent>(entity);
				entityJson["image"] = {
					{"texturePath",img.texturePath},
					{"color",vec4ToJson(img.color)},
					{"visible",img.visible}
				};
			}

			// Button
			if (m_scene.registry().all_of<ButtonComponent>(entity))
			{
				auto& btn = m_scene.registry().get<ButtonComponent>(entity);
				entityJson["button"] = {
					{"interactable",btn.interactable},
					{"normalColor",vec4ToJson(btn.normalColor)},
					{"hoveredColor",vec4ToJson(btn.hoveredColor)},
					{"pressedColor",vec4ToJson(btn.pressedColor)}
				};
			}

			// LightComponent
			if (m_scene.registry().all_of<LightComponent>(entity))
			{
				auto& lc = m_scene.registry().get<LightComponent>(entity);
				entityJson["light"] = {
					{"type",static_cast<int>(lc.type)},
					{"color",vec3ToJson(lc.color)},
					{"intensity",lc.intensity},
					{"range",lc.range},
					{"spotInner",lc.spotInner},
					{"spotOuter",lc.spotOuter},
					{"enable",lc.enable},
					{"castShadow",lc.castShadow},
					{"softShadow",lc.softShadow},
					{"shadowBias",lc.shadowBias},
					{"pcfRadius",lc.pcfRadius},
				};
			}

			// SkySphere
			if (m_scene.registry().all_of<SkySphereComponent>(entity))
			{
				auto& sky = m_scene.registry().get<SkySphereComponent>(entity);
				entityJson["skySphere"] =
				{
					{"texturePath" , sky.texturePath},
					{"topColor" , vec4ToJson(sky.topColor)},
					{"horizonColor" , vec4ToJson(sky.horizonColor)},
					{"bottomColor" , vec4ToJson(sky.bottomColor)},
					{"exposure" , sky.exposure},
					{"enabled" , sky.enabled},
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
	bool SceneSerializer::deserialize(const std::string& path)
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

		std::vector<entt::entity> toDestroy;
		{
			auto existing = m_scene.registry().view<TagComponent>();
			toDestroy.reserve(existing.size());
			for (auto entity : existing)
				toDestroy.push_back(entity);
		}

		for (auto entity : toDestroy)
		{
			if (!m_scene.registry().valid(entity)) continue;
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

			
			auto& tagComp = entity.getComponent<TagComponent>();
			tagComp.category = entityJson.value("category", "Untagged");


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
				mesh.materialPath = mj.value("materialPath", "");
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

			// Animator
			if (entityJson.contains("animator")) {
				auto& aj = entityJson["animator"];
				auto& anim = entity.addComponent<AnimatorComponent>();
				anim.currentClipName = aj.value("currentClipName", "");
				anim.playbackSpeed = aj.value("playbackSpeed", 1.0f);
				anim.playing = aj.value("playing", true);
				anim.loop = aj.value("loop", true);
			}

			//====== UI =======
			// Canvas
			if (entityJson.contains("canvas"))
			{
				auto& cj = entityJson["canvas"];
				auto& canvas = entity.addComponent<CanvasComponent>();
				canvas.renderMode = static_cast<CanvasRenderMode>(cj.value("renderMode",0));
				canvas.referenceResolution = vec2FromJson(cj["referenceResolution"], { 1920.0f,1080.0f });
				canvas.sortOrder = cj.value("sortOrder", 0);
				canvas.enabled = cj.value("enabled", true);
			}

			// RectTransform
			if (entityJson.contains("rectTransform"))
			{
				auto& rj = entityJson["rectTransform"];
				auto& rt = entity.addComponent<RectTransformComponent>();
				rt.anchorMin = vec2FromJson(rj["anchorMin"], { 0.5f,0.5f });
				rt.anchorMax = vec2FromJson(rj["anchorMax"], { 0.5f,0.5f });
				rt.anchoredPos = vec2FromJson(rj["anchoredPos"], { 0.0f,0.0f });
				rt.sizeDelta = vec2FromJson(rj["sizeDelta"], { 100.0f,100.0f });
				rt.pivot = vec2FromJson(rj["pivot"], { 0.5f,0.5f });
				rt.rotation = rj.value("rotation", 0.0f);
				rt.scale = vec2FromJson(rj["scale"], { 1.0f,1.0f });
			}

			// Image
			if (entityJson.contains("image"))
			{
				auto& ij = entityJson["image"];
				auto& img = entity.addComponent<ImageComponent>();
				img.texturePath = ij.value("texturePath", "");
				img.color = vec4FromJson(ij["color"]);
				img.visible = ij.value("visible", true);
			}

			// Button
			if (entityJson.contains("button"))
			{
				auto& bj = entityJson["button"];
				auto& btn = entity.addComponent<ButtonComponent>();
				btn.interactable = bj.value("intaractable", true);
				btn.normalColor = vec4FromJson(bj["normalColor"]);
				btn.hoveredColor = vec4FromJson(bj["hoveredColor"]);
				btn.pressedColor = vec4FromJson(bj["pressedColor"]);
			}
			
			// LightComponent
			if (entityJson.contains("light")) {
				auto& lj = entityJson["light"];
				auto& lc = entity.addComponent<LightComponent>();
				lc.type = static_cast<LightType>(lj.value("type", 0));
				lc.color = vec3FromJson(lj["color"]);
				lc.intensity = lj.value("intensity", 1.0f);
				lc.range = lj.value("range", 10.0f);
				lc.spotInner = lj.value("spotInner", 20.0f);
				lc.spotOuter = lj.value("spotOuter", 30.0f);
				lc.enable = lj.value("enable", true);
				lc.castShadow = lj.value("castShadow", true);
				lc.softShadow = lj.value("softShadow", true);
				lc.shadowBias = lj.value("shadowBias", 0.005f);
				lc.pcfRadius = lj.value("pcfRadius", 1.5f);
			}

			// SkySphereComponent
			if (entityJson.contains("skySphere")) {
				auto& sj = entityJson["skySphere"];
				auto& sky = entity.addComponent<SkySphereComponent>();
				sky.texturePath = sj.value("texturePath", "");
				sky.topColor = vec4FromJson(sj["topColor"]);
				sky.horizonColor = vec4FromJson(sj["horizonColor"]);
				sky.bottomColor = vec4FromJson(sj["bottomColor"]);
				sky.exposure = sj.value("exposure", 1.0f);
				sky.enabled = sj.value("enabled", true);
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
