#include "InspectorPanel.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "asset/loaders/MaterialLoader.h"
#include "asset/loaders/AnimationCache.h"
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
		drawSkyComponent(scene, selected);

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

			if (!m.meshPath.empty() && !meshExists)
			{
				ImGui::PopStyleColor();
				ImGui::TextColored({ 1.0f,0.3f,0.3f,1.0f }, " File not Found");
			}

			//===== Drag & Drop ====
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("ASSET_PATH")) {
					
						m.meshPath = static_cast<const char*>(payload->Data);
						m.cachedMesh = nullptr;

						// Animation付きのモデルの場合Animatorを自動追加
						auto& clips = FaluEngine::AnimationCache::get().getAnimations(m.meshPath);
						if (!clips.empty())
						{
							FaluEngine::Entity e(entity, scene);
							if (!e.hasComponent<FaluEngine::AnimatorComponent>())
							{
								auto& animator = e.addComponent<FaluEngine::AnimatorComponent>();
								animator.currentClipName = clips[0]->name.c_str();
								animator.playing = true;
								animator.loop = true;
							}
						}
				}
				ImGui::EndDragDropTarget();
			}

			//====== Material Path =======
			ImGui::Text("Material Path");
			ImGui::SameLine();
			char matBuf[512];
			strncpy_s(matBuf, m.materialPath.c_str(), sizeof(matBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##MatPath", matBuf, sizeof(matBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m.materialPath = matBuf;
				m.cachedMaterial = nullptr;
			}

			// Drag & Drop
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					m.materialPath = static_cast<const char*>(payload->Data);
					m.cachedMaterial = nullptr;
				}
				ImGui::EndDragDropTarget();
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

			if (m.cachedMaterial && m.cachedMaterial->valid)
			{
				drawMaterialEditor(m.cachedMaterial.get(), m.materialPath);
			}
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
				ImGui::DragFloat("Radius", &rb.radius, 0.01f, 0.01f, 100.0f);
			if (rb.shape == FaluEngine::ColliderShape::Capsule)
				ImGui::DragFloat("Height", &rb.height, 0.01f, 0.01f, 100.0f);

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

	void InspectorPanel::drawSkyComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::SkySphereComponent>(entity)) return;

		if (ImGui::CollapsingHeader("SkySphere", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& sky = scene->registry().get<FaluEngine::SkySphereComponent>(entity);

			ImGui::Text("Texture");
			ImGui::SameLine();
			char buf[512];
			strncpy_s(buf, sky.texturePath.c_str(), sizeof(buf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##SkyTex", buf, sizeof(buf),
				ImGuiInputTextFlags_EnterReturnsTrue)) {
				sky.texturePath = buf;
				sky.cachedTexture = nullptr;
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("ASSET_PATH")) {
					std::string droppedPath = static_cast<const char*>(payload->Data);
					sky.texturePath = droppedPath;
					sky.cachedTexture = nullptr;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Separator();
			ImGui::Text("Gradient (used when no texture)");
			ImGui::ColorEdit4("Top", glm::value_ptr(sky.topColor));
			ImGui::ColorEdit4("Horizon", glm::value_ptr(sky.horizonColor));
			ImGui::ColorEdit4("Bottom", glm::value_ptr(sky.bottomColor));
			ImGui::DragFloat("Exposure", &sky.exposure, 0.01f, 0.0f, 10.0f);
			ImGui::Checkbox("Enabled", &sky.enabled);
		}
	}

	void InspectorPanel::drawMaterialEditor(FaluEngine::MaterialAsset* material, const std::string& materialPath)
	{
		if (!material) return;

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Mateiral (PBR)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool changed = false;

			//=== Albedo ====
			ImGui::Text("Albedo Color");
			if (ImGui::ColorEdit4("##Albedo", glm::value_ptr(material->albedoColor)))
				changed = true;

			ImGui::Text("Albedo Map");
			ImGui::SameLine();
			char albedoBuf[512];
			strncpy_s(albedoBuf, material->albedoMapPath.c_str(), sizeof(albedoBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##AlbedoMap", albedoBuf, sizeof(albedoBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				material->albedoMapPath = albedoBuf;
				material->cachedAlbedoMap = nullptr;
				changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					material->albedoMapPath = static_cast<const char*>(payload->Data);
					material->cachedAlbedoMap = nullptr;
					changed = true;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Separator();

			//=== Metallic / Roughness ====
			if (ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f)) changed = true;
			if (ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f)) changed = true;

			ImGui::Text("Metallic/Roughness Map (R=Metal,G=Rough)");
			ImGui::SameLine();
			char metalBuf[512];
			strncpy_s(metalBuf, material->metallicMapPath.c_str(), sizeof(metalBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##MetallicMap", metalBuf, sizeof(metalBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				material->metallicMapPath = metalBuf;
				material->cachedMetallicMap = nullptr;
				changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					material->metallicMapPath = static_cast<const char*>(payload->Data);
					material->cachedMetallicMap = nullptr;
					changed = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::Separator();

			//==== Normal Map =====
			ImGui::Text("Normal Map");
			ImGui::SameLine();
			char normalBuf[512];
			strncpy_s(normalBuf, material->normalMapPath.c_str(), sizeof(normalBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##NormalMap", normalBuf, sizeof(normalBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				material->normalMapPath = normalBuf;
				material->cachedNormalMap = nullptr;
				changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					material->normalMapPath = static_cast<const char*>(payload->Data);
					material->cachedNormalMap = nullptr;
					changed = true;
				}
				ImGui::EndDragDropTarget();
			}

			//==== AO Map =====
			ImGui::Text("AO Map");
			ImGui::SameLine();
			char aoBuf[512];
			strncpy_s(aoBuf, material->aoMapPath.c_str(), sizeof(aoBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##AOMap", aoBuf, sizeof(aoBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				material->aoMapPath = aoBuf;
				material->cachedAOMap = nullptr;
				changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					material->aoMapPath = static_cast<const char*>(payload->Data);
					material->cachedAOMap = nullptr;
					changed = true;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Separator();

			//==== Emissive =====
			if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(material->emissiveColor)))
				changed = true;
			if (ImGui::DragFloat("Emissive Strength", &material->emissiveStrength, 0.01f, 0.0f, 50.0f))
				changed = true;

			ImGui::Text("Emissive Map");
			ImGui::SameLine();
			char emissiveBuf[512];
			strncpy_s(emissiveBuf, material->emissiveMapPath.c_str(), sizeof(emissiveBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##EmissiveMap", emissiveBuf, sizeof(emissiveBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				material->emissiveMapPath = emissiveBuf;
				material->cachedEmissiveMap = nullptr;
				changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					material->emissiveMapPath = static_cast<const char*>(payload->Data);
					material->cachedEmissiveMap = nullptr;
					changed = true;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::Separator();

			//=== Custom Shader ====
			ImGui::Text("Vertex Shader (optional)");
			ImGui::SameLine();
			char vsBuf[512];
			strncpy_s(vsBuf, material->vertexShaderPath.c_str(), sizeof(vsBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("###VSPath", vsBuf, sizeof(vsBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				material->vertexShaderPath = vsBuf;
				material->cachedShader = nullptr;
				changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					material->vertexShaderPath = static_cast<const char*>(payload->Data);
					material->cachedShader = nullptr;
					changed = true;
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Text("Pixel Shader (optional)");
			ImGui::SameLine();
			char psBuf[512];
			strncpy_s(psBuf, material->pixelShaderPath.c_str(), sizeof(psBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("###PSPath", psBuf, sizeof(psBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				material->pixelShaderPath = psBuf;
				material->cachedShader = nullptr;
				changed = true;
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					material->pixelShaderPath = static_cast<const char*>(payload->Data);
					material->cachedShader = nullptr;
					changed = true;
				}
				ImGui::EndDragDropTarget();
			}

			//=== Save Button =====
			ImGui::Spacing();
			if (ImGui::Button("Save Material", { -1,0 }))
			{
				if (!materialPath.empty())
				{
					FaluEngine::saveMaterial(materialPath, *material);
				}
			}

			if (changed)
			{
				ImGui::TextColored({ 1.0f,0.8f,0.3f,1.0f },
					"Modified (not saved yet)");
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
			{
				if (e.hasComponent<FaluEngine::CanvasComponent>() ||
					e.hasComponent<FaluEngine::RectTransformComponent>())
				{
					LOG_WARN("MeshComponent should not be added to a UI entity!");
				}
				else
					e.addComponent<FaluEngine::MeshComponent>();
			}
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

		if (!scene->registry().all_of<FaluEngine::SkySphereComponent>(entity))
		{
			if (ImGui::MenuItem("SkySphere Component"))
				e.addComponent<FaluEngine::SkySphereComponent>();
		}

		ImGui::EndPopup();
	}
}
