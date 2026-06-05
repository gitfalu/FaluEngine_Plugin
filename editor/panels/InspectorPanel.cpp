#include "InspectorPanel.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "physics/RigidbodyComponent.h"
#include "core/PathResolver.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <filesystem>

namespace Editor
{
	void InspectorPanel::draw(FaluEngine::Scene* scene, entt::entity selected)
	{
		ImGui::Begin("Inspector");

		if (!scene || selected == entt::null)
		{
			ImGui::TextDisabled("no entity selected");
			ImGui::End();
			return;
		}

		auto& tag = scene->registry().get<FaluEngine::TagComponent>(selected);
		char buf[256];
		strncpy_s(buf,tag.name.c_str(), sizeof(buf));
		if (ImGui::InputText("##Name", buf, sizeof(buf)))
			tag.name = buf;

		ImGui::SameLine();
		ImGui::TextDisabled("ID: %u", static_cast<uint32_t>(selected));
		ImGui::Separator();

		drawTransformComponent(scene, selected);
		drawMeshComponent(scene, selected);
		drawCameraComponent(scene, selected);
		drawRigidbodyComponent(scene, selected);
		drawScriptComponent(scene, selected);
		drawLightComponent(scene, selected);

		ImGui::Spacing();

		float buttonWidth = ImGui::GetContentRegionAvail().x;
		if (ImGui::Button("Add Component", { buttonWidth ,0 }))
			ImGui::OpenPopup("AddComponent");

		drawAddComponentMenu(scene, selected);

		ImGui::End();
	}

	void InspectorPanel::drawTransformComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::TransformComponent>(entity)) return;

		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& t = scene->registry().get<FaluEngine::TransformComponent>(entity);

			ImGui::DragFloat3("Position", glm::value_ptr(t.position), 0.1f);

			glm::vec3 eular = glm::degrees(glm::eulerAngles(t.rotation));
			if (ImGui::DragFloat3("Rotation", glm::value_ptr(eular), 0.5f))
				t.rotation = glm::quat(glm::radians(eular));

			ImGui::DragFloat3("Scale", glm::value_ptr(t.scale), 0.01f, 0.001f, 100.0f);
		}
	}

	void InspectorPanel::drawMeshComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::MeshComponent>(entity)) return;
		
		if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& m = scene->registry().get<FaluEngine::MeshComponent>(entity);

			//==== Mesh Path ====

			ImGui::Text("Mesh Path");
			ImGui::SameLine();

			// ファイルの存在を確認
			bool meshExists = std::filesystem::exists(m.meshPath);
			if (!m.meshPath.empty() && !meshExists)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f,0.3f,0.3f,1.0f });
				ImGui::PopStyleColor();
				ImGui::TextColored({ 1.0f,0.3f,0.3f,1.0f }, " File not Found");
			}

			char meshBuf[512];
			strncpy_s(meshBuf, m.meshPath.c_str(), sizeof(meshBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##MeshPath", meshBuf, sizeof(meshBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				auto fullpath = FaluEngine::PathResolver::resolveStr(meshBuf);
				m.meshPath = fullpath;
				m.cachedMesh = nullptr;// キャッシュをリセット
			}


			//==== Texture Path =====
			ImGui::Text("Texture");
			ImGui::SameLine();

			bool texExists = std::filesystem::exists(m.texturePath);
			if (!m.texturePath.empty() && !texExists)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f,0.3f,0.3f,1.0f });
				ImGui::PopStyleColor();
				ImGui::TextColored({ 1.0f,0.3f,0.3f,1.0f }, " File not found");
			}

			char texBuf[512];
			strncpy_s(texBuf, m.texturePath.c_str(), sizeof(texBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##TexPath", texBuf, sizeof(texBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				auto fullpath = FaluEngine::PathResolver::resolveStr(texBuf);
				m.texturePath = fullpath;
				m.cachedTexture = nullptr;
			}

			//=== Normal Map Path =====
			ImGui::Text("Normal Map");
			ImGui::SameLine();

			bool normalExists = m.normalMapPath.empty() ||
				std::filesystem::exists(m.normalMapPath);
			if (!m.normalMapPath.empty() && !normalExists)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f,0.3f,0.3f,1.0f });
				ImGui::PopStyleColor();
				ImGui::TextColored({ 1.0f,0.3f,0.3f,1.0f }, " File not found");
			}

			char normalBuf[512];
			strncpy_s(normalBuf, m.normalMapPath.c_str(), sizeof(normalBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##NormalPath", normalBuf, sizeof(normalBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				auto fullpath = FaluEngine::PathResolver::resolveStr(normalBuf);
				m.normalMapPath = fullpath;
				m.cachedNormalMap = nullptr;
			}


			//===== Mesh Info ====
			if (m.cachedMesh)
			{
				ImGui::Separator();
				ImGui::TextDisabled("Vertices: %zu Indices: %zu SubMeshes: %zu",
					m.cachedMesh->vertices.size(),
					m.cachedMesh->indices.size(),
					m.cachedMesh->subMeshes.size());
			}

			ImGui::Checkbox("Visible", &m.visible);
		}
	}

	void InspectorPanel::drawCameraComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::CameraComponent>(entity)) return;

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& cam = scene->registry().get<FaluEngine::CameraComponent>(entity);

			float fov = cam.camera.getFovDeg();
			if (ImGui::DragFloat("FOV", &fov, 0.5f, 10.0f, 170.0f))
				cam.camera.setPerspective(fov, cam.camera.getAspectRatio(),
					cam.camera.getNearClip(), cam.camera.getFarClip());

			float nearClip = cam.camera.getNearClip();
			float farClip = cam.camera.getFarClip();
			if (ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.001f, 10.0f))
				cam.camera.setPerspective(fov, cam.camera.getAspectRatio(), nearClip, farClip);
			if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, 10.0f, 10000.0f))
				cam.camera.setPerspective(fov, cam.camera.getAspectRatio(), nearClip, farClip);

			ImGui::Checkbox("Primary", &cam.isPrimary);
		}
	}

	void InspectorPanel::drawRigidbodyComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::RigidbodyComponent>(entity)) return;

		if (ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& rb = scene->registry().get<FaluEngine::RigidbodyComponent>(entity);

			const char* bodyTypes[] = { "Static","Dynamic","Kinematic" };
			int bodyType = static_cast<int>(rb.bodyType);
			if (ImGui::Combo("Body Type", &bodyType, bodyTypes, 3))
				rb.bodyType = static_cast<FaluEngine::BodyType>(bodyType);

			const char* shapes[] = { "Box","Sphere","Capsule" };
			int shape = static_cast<int>(rb.shape);
			if (ImGui::Combo("Shape", &shape, shapes, 3))
				rb.shape = static_cast<FaluEngine::ColliderShape>(shape);

			if (rb.shape == FaluEngine::ColliderShape::Box)
				ImGui::DragFloat3("half Extents", glm::value_ptr(rb.halfExtents),0.01f,0.01f,100.0f);
			if (rb.shape == FaluEngine::ColliderShape::Sphere || 
				rb.shape == FaluEngine::ColliderShape::Capsule)
				ImGui::DragFloat3("Radius", &rb.radius, 0.01f, 0.01f, 100.0f);
			if (rb.shape == FaluEngine::ColliderShape::Capsule)
				ImGui::DragFloat3("Height", &rb.height, 0.01f, 0.01f, 100.0f);

			ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 1000.0f);
			ImGui::DragFloat("Restitution", &rb.restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &rb.friction, 0.01f, 0.0f, 1.0f);
			ImGui::Checkbox("Use Gravity", &rb.useGravity);

			ImGui::TextDisabled("Registered: %s", rb.registered ? "Yes" : "No");
		}
	}

	void InspectorPanel::drawScriptComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::ScriptComponent>(entity)) return;

		if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& sc = scene->registry().get<FaluEngine::ScriptComponent>(entity);
			ImGui::Text("Script: %s", sc.scriptPath.empty() ? "(none)" : sc.scriptPath.c_str());
			ImGui::TextDisabled("Initialized: %s", sc.scriptPath.empty() ? "No" : (sc.instance ? "Yes" : "No"));
		}
	}

	void InspectorPanel::drawLightComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::LightComponent>(entity))return;

		if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto& lc = scene->registry().get<FaluEngine::LightComponent>(entity);

			const char* types[] = { "Directional","Point","Spot" };
			int type = static_cast<int>(lc.type);
			if (ImGui::Combo("Type", &type, types, 3))
				lc.type = static_cast<FaluEngine::LightType>(type);

			ImGui::ColorEdit3("Color", glm::value_ptr(lc.color));
			ImGui::DragFloat("Intensity", &lc.intensity, 0.01f, 0.0f, 100.0f);

			if (lc.type != FaluEngine::LightType::Directional)
				ImGui::DragFloat("Range", &lc.range, 0.1f, 0.0f, 1000.0f);

			if (lc.type == FaluEngine::LightType::Spot)
			{
				ImGui::DragFloat("Inner Angle", &lc.spotInner, 0.5f, 0.0f, 90.0f);
				ImGui::DragFloat("Outer Angle", &lc.spotOuter, 0.5f, 0.0f, 90.0f);
			}

			ImGui::Checkbox("Enabled", &lc.enable);

			ImGui::Separator();
			ImGui::Text("Shadow");
			ImGui::Checkbox("Cast Shadow", &lc.castShadow);
			if (lc.castShadow)
			{
				ImGui::Checkbox("Soft Shadow", &lc.softShadow);
				ImGui::DragFloat("Bias", &lc.shadowBias, 0.0001f, 0.0f, 0.1f, "%.4f");
				ImGui::DragFloat("PCF Radius", &lc.pcfRadius, 0.1f, 0.0f, 5.0f);
			}
		}
	}

	void InspectorPanel::drawAddComponentMenu(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!ImGui::BeginPopup("AddComponent")) return;

		FaluEngine::Entity e(entity, scene);

		if(!scene->registry().all_of<FaluEngine::MeshComponent>(entity))
		{ 
			if (ImGui::MenuItem("Mesh Component"))
				e.addComponent<FaluEngine::MeshComponent>();
		}

		if (!scene->registry().all_of<FaluEngine::CameraComponent>(entity))
		{
			if (ImGui::MenuItem("Camera Component"))
				e.addComponent<FaluEngine::CameraComponent>();
		}

		if (!scene->registry().all_of<FaluEngine::RigidbodyComponent>(entity))
		{
			if (ImGui::MenuItem("Rigidbody Component"))
				e.addComponent<FaluEngine::RigidbodyComponent>();
		}

		if (!scene->registry().all_of<FaluEngine::ScriptComponent>(entity))
		{
			if (ImGui::MenuItem("Script Component"))
				e.addComponent<FaluEngine::ScriptComponent>();
		}

		if (!scene->registry().all_of<FaluEngine::LightComponent>(entity))
		{
			if (ImGui::MenuItem("Light Component"))
				e.addComponent<FaluEngine::LightComponent>();
		}

		ImGui::EndPopup();
	}
}
