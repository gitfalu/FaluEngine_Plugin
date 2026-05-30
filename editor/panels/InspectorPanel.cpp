#include "InspectorPanel.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "physics/RigidbodyComponent.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>


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

			ImGui::Text("Mesh: %s", m.meshPath.empty() ? "(none)" : m.meshPath.c_str());
			ImGui::Text("Texture: %s", m.texturePath.empty() ? "(none)" : m.texturePath.c_str());
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
