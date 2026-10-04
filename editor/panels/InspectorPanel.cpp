#include "InspectorPanel.h"
#include "FaluEngine/Scene.h"
#include "FaluEngine/Entity.h"
#include "FaluEngine/Component.h"
#include "FaluEngine/Localization.h"
#include "scene/SceneManager.h"
#include "asset/loaders/MaterialLoader.h"
#include "asset/loaders/AnimationCache.h"
#include "asset/loaders/MeshLoader.h"
#include "physics/RigidbodyComponent.h"
#include "terrain/TerrainComponent.h"
#include "terrain/TerrainFactory.h"
#include "tools/TerrainSculptTool.h"
#include "core/PathResolver.h"
#include "FaluEngine/UITypes.h"
#include "audio/AudioClip.h"
#include "audio/AudioEngine.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <filesystem>

namespace Editor
{
	void InspectorPanel::draw(FaluEngine::Scene* scene, entt::entity selected,ID3D11Device* device)
	{
		ImGui::Begin("Inspector");

		if (!scene || selected == entt::null)
		{
			ImGui::TextDisabled(TR("no entity selected"));
			ImGui::End();
			return;
		}

		if (Editor::TerrainSculptTool::get().enabled &&
			Editor::TerrainSculptTool::get().getTargetEntity() != selected)
		{
			Editor::TerrainSculptTool::get().clearTarget();
		}

		auto& tag = scene->registry().get<FaluEngine::TagComponent>(selected);
		char buf[256];
		strncpy_s(buf,tag.name.c_str(), sizeof(buf));
		if (ImGui::InputText("##Name", buf, sizeof(buf)))
			tag.name = buf;

		ImGui::SameLine();
		ImGui::TextDisabled("ID: %u", static_cast<uint32_t>(selected));

		char catbuf[128];
		strncpy_s(catbuf, tag.category.c_str(), sizeof(catbuf));
		ImGui::SetNextItemWidth(150.0f);
		if (ImGui::InputText(TR("Category (Tag)"), catbuf, sizeof(catbuf)))
			tag.category = catbuf;

		ImGui::Separator();

		drawTransformComponent(scene, selected);
		drawMeshComponent(scene, selected);
		drawCameraComponent(scene, selected);
		drawRigidbodyComponent(scene, selected);
		drawTerrainComponent(scene, selected, device);

		//==== Script ====
		drawScriptComponent(scene, selected);
		drawNativeScriptComponent(scene, selected);

		drawLightComponent(scene, selected);
		drawSkyComponent(scene, selected);
		drawAudioComponent(scene, selected);

		//===== UI ======
		drawRectTransformComponent(scene, selected);
		drawCanvasComponent(scene, selected);
		drawImageComponent(scene, selected);
		drawButtonComponent(scene, selected);

		ImGui::Spacing();

		float buttonWidth = ImGui::GetContentRegionAvail().x;
		if (ImGui::Button(TR("Add Component"), { buttonWidth ,0 }))
			ImGui::OpenPopup("AddComponent");

		drawAddComponentMenu(scene, selected,device);

		if (ImGui::IsAnyItemActive())
			FaluEngine::SceneManager::get().markDirty();

		ImGui::End();
	}

	void InspectorPanel::drawTransformComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::TransformComponent>(entity)) return;

		if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen))
		{

			auto& t = scene->registry().get<FaluEngine::TransformComponent>(entity);

			ImGui::DragFloat3(TR("Position"), glm::value_ptr(t.position), 0.1f);

			
			glm::vec3 eular = t.rotationEulerHint;
			if (ImGui::DragFloat3(TR("Rotation"), glm::value_ptr(eular), 0.5f))
			{
				t.setRotationEuler(eular);
			}

			ImGui::DragFloat3(TR("Scale"), glm::value_ptr(t.scale), 0.01f, 0.001f, 100.0f);
		}
	}

	void InspectorPanel::drawMeshComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::MeshComponent>("Mesh", scene, entity)) return;
		{
			auto& m = scene->registry().get<FaluEngine::MeshComponent>(entity);

			//==== Mesh Path ====

			ImGui::Text(TR("Mesh Path"));
			ImGui::SameLine();

			// ファイルの存在を確認
			bool meshExists = std::filesystem::exists(
				FaluEngine::PathResolver::resolve(m.meshPath));
			bool showError = !meshExists && !m.meshPath.empty();
			if (showError)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, { 1.0f,0.3f,0.3f,1.0f });
			}

			char meshBuf[512];
			strncpy_s(meshBuf, m.meshPath.c_str(), sizeof(meshBuf));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##MeshPath", meshBuf, sizeof(meshBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m.meshPath = meshBuf;
				m.cachedMesh = nullptr;// キャッシュをリセット
			}

			if (showError)
			{
				ImGui::PopStyleColor();
				ImGui::TextColored({ 1.0f,0.3f,0.3f,1.0f }, " File not Found");
			}

			//===== Drag & Drop ====
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("ASSET_PATH")) {
					
					const char* payloadPath = static_cast<const char*>(payload->Data);
					
					const std::string assetPath = FaluEngine::PathResolver::normalizeAssetPath(payloadPath);

					const auto fsPath = FaluEngine::PathResolver::resolve(assetPath);

					if (std::filesystem::exists(fsPath))
					{
						m.meshPath = assetPath;
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
				}
				ImGui::EndDragDropTarget();
			}

			//====== Material Path =======
			ImGui::Text(TR("Material Path"));
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
					const char* payloadPath = static_cast<const char*>(payload->Data);

					const std::string assetPath = FaluEngine::PathResolver::normalizeAssetPath(payloadPath);

					const auto fsPath = FaluEngine::PathResolver::resolve(assetPath);

					if (std::filesystem::exists(fsPath))
					{
						m.materialPath = assetPath;
						m.cachedMaterial = nullptr;
					}
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

			ImGui::Checkbox(TR("Visible"), &m.visible);

			if (m.cachedMaterial && m.cachedMaterial->valid)
			{
				drawMaterialEditor(m.cachedMaterial.get(), m.materialPath);
			}
		}
	}

	void InspectorPanel::drawCameraComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::CameraComponent>("Camera", scene, entity)) return;
		{
			auto& cam = scene->registry().get<FaluEngine::CameraComponent>(entity);

			float fov = cam.camera.getFovDeg();
			if (ImGui::DragFloat(TR("FOV"), &fov, 0.5f, 10.0f, 170.0f))
				cam.camera.setPerspective(fov, cam.camera.getAspectRatio(),
					cam.camera.getNearClip(), cam.camera.getFarClip());

			float nearClip = cam.camera.getNearClip();
			float farClip = cam.camera.getFarClip();
			if (ImGui::DragFloat(TR("Near Clip"), &nearClip, 0.01f, 0.001f, 10.0f))
				cam.camera.setPerspective(fov, cam.camera.getAspectRatio(), nearClip, farClip);
			if (ImGui::DragFloat(TR("Far Clip"), &farClip, 1.0f, 10.0f, 10000.0f))
				cam.camera.setPerspective(fov, cam.camera.getAspectRatio(), nearClip, farClip);

			ImGui::Checkbox("Primary", &cam.isPrimary);
		}
	}

	void InspectorPanel::drawRigidbodyComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::RigidbodyComponent>("Rigidbody", scene, entity)) return;
		{
			auto& rb = scene->registry().get<FaluEngine::RigidbodyComponent>(entity);

			const char* bodyTypes[] = { "Static","Dynamic","Kinematic" };
			int bodyType = static_cast<int>(rb.bodyType);
			if (ImGui::Combo("Body Type", &bodyType, bodyTypes, 3))
				rb.bodyType = static_cast<FaluEngine::BodyType>(bodyType);

			const char* shapes[] = { "Box","Sphere","Capsule" };
			int shape = static_cast<int>(rb.shape);
			if (ImGui::Combo(TR("Shape"), &shape, shapes, 3))
				rb.shape = static_cast<FaluEngine::ColliderShape>(shape);

			if (rb.shape == FaluEngine::ColliderShape::Box)
				ImGui::DragFloat3(TR("half Extents"), glm::value_ptr(rb.halfExtents),0.01f,0.01f,100.0f);
			if (rb.shape == FaluEngine::ColliderShape::Sphere || 
				rb.shape == FaluEngine::ColliderShape::Capsule)
				ImGui::DragFloat(TR("Radius"), &rb.radius, 0.01f, 0.01f, 100.0f);
			if (rb.shape == FaluEngine::ColliderShape::Capsule)
				ImGui::DragFloat(TR("Height"), &rb.height, 0.01f, 0.01f, 100.0f);

			ImGui::DragFloat(TR("Mass"), &rb.mass, 0.1f, 0.01f, 1000.0f);
			ImGui::DragFloat(TR("Restitution"), &rb.restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat(TR("Friction"), &rb.friction, 0.01f, 0.0f, 1.0f);
			ImGui::Checkbox(TR("Use Gravity"), &rb.useGravity);

			ImGui::TextDisabled(TR("Registered: %s"), rb.registered ? "Yes" : "No");
		}
	}

	void InspectorPanel::drawTerrainComponent(FaluEngine::Scene* scene, entt::entity entity, ID3D11Device* device)
	{
		if (!drawComponentHeader<FaluEngine::TerrainComponent>("Terrain", scene, entity)) return;
		{
			FaluEngine::Entity e(entity, scene);
			auto& terrain = scene->registry().get<FaluEngine::TerrainComponent>(entity);

			bool changed = false;

			int resX = static_cast<int>(terrain.resolutionX);
			if (ImGui::DragInt(TR("Resolution X"), &resX, 1.0f, 2, 512))
			{
				terrain.resolutionX = static_cast<uint32_t>(resX);
				changed = true;
			}

			int resZ = static_cast<int>(terrain.resolutionZ);
			if (ImGui::DragInt(TR("Resolution Z"), &resZ, 1.0f, 2, 512))
			{
				terrain.resolutionZ = static_cast<uint32_t>(resZ);
				changed = true;
			}

			if (terrain.resolutionX != terrain.resolutionZ)
			{
				ImGui::TextColored({ 1.0f,0.6f,0.2f,1.0f },
					"HeightField requires a square grid. Will be clamped to %u on regenerate.",
					(std::min)(terrain.resolutionX,terrain.resolutionZ)
				);
			}

			changed |= ImGui::DragFloat(TR("Size X"), &terrain.sizeX, 0.5f, 1.0f, 10000.0f);
			changed |= ImGui::DragFloat(TR("Size Z"), &terrain.sizeZ, 0.5f, 1.0f, 10000.0f);
			changed |= ImGui::DragFloat(TR("Height Scale"), &terrain.heightScale, 0.1f, 0.0f, 1000.0f);

			ImGui::Separator();
			ImGui::TextDisabled(TR("Noise"));

			changed |= ImGui::DragFloat(TR("Frequency"), &terrain.noiseFrequency, 0.001f, 0.0001f, 1.0f, "%.4f");
			changed |= ImGui::DragInt(TR("Octaves"), &terrain.octaves, 1.0f, 1, 8);
			changed |= ImGui::DragFloat(TR("Lacunarity"), &terrain.lacunarity, 0.05f, 1.0f, 4.0f);
			changed |= ImGui::DragFloat(TR("Persistence"), &terrain.persistence, 0.01f, 0.0f, 1.0f);

			int seed = static_cast<int>(terrain.seed);
			if (ImGui::DragInt("Seed", &seed, 1.0f, 0, INT32_MAX))
			{
				terrain.seed = static_cast<uint32_t>(seed);
				changed = true;
			}

			ImGui::Separator();

			ImGui::TextDisabled(TR("Sculpt (SceneView)"));

			bool sculptEnabled = Editor::TerrainSculptTool::get().enabled &&
				Editor::TerrainSculptTool::get().getTargetEntity() == entity;

			if (ImGui::Checkbox(TR("Sculpt Mode"), &sculptEnabled))
			{
				if (sculptEnabled)
				{
					Editor::TerrainSculptTool::get().setTarget(scene, entity);
					Editor::TerrainSculptTool::get().enabled = true;
				}
				else
				{
					Editor::TerrainSculptTool::get().clearTarget();
				}
			}

			if (sculptEnabled)
			{
				auto& tool = Editor::TerrainSculptTool::get();

				const char* brushModes[] = { "Raise","Lower","Flatten","Smooth" };
				int brushMode = static_cast<int>(tool.mode);
				if (ImGui::Combo(TR("Brush Mode"), &brushMode, brushModes, 4))
				{
					tool.mode = static_cast<Editor::BrushMode>(brushMode);
				}

				ImGui::DragFloat(TR("Brush Radius"), &tool.radius, 0.1f, 0.1f, 500.0f);
				ImGui::DragFloat(TR("Brush Strength"), &tool.strength, 0.05f, 0.01f, 100.0f);

				ImGui::TextDisabled(TR("SceneView上でLMBドラッグで編集 / Shiftで反転(Raise/Lowerのみ)"));

				glm::vec3 hitPos;
				if (tool.getLastHit(hitPos))
				{
					ImGui::TextDisabled(TR("Cursor: (%.1f,%.1f,%.1f)"), hitPos.x, hitPos.y, hitPos.z);
				}
			}

			ImGui::Separator();

			if (terrain.dirty)
			{
				ImGui::TextColored({ 1.0f,0.8f,0.2f,1.0f }, "Parameters changed - not regenated yet.");
			}

			float buttonWidth = ImGui::GetContentRegionAvail().x;
			if (ImGui::Button(TR("Regenerate"), { buttonWidth,0 }))
			{
				FaluEngine::regenerateTerrain(*scene, e, device);
				FaluEngine::SceneManager::get().markDirty();
			}

			ImGui::TextDisabled(TR("Vertices: %u x %u"), terrain.vertexCountX(), terrain.vertexCountZ());
		}
	}

	void InspectorPanel::drawScriptComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::ScriptComponent>("Script", scene, entity)) return;
		{
			auto& sc = scene->registry().get<FaluEngine::ScriptComponent>(entity);
			ImGui::Text(TR("Script: %s"), sc.scriptPath.empty() ? "(none)" : sc.scriptPath.c_str());
			ImGui::TextDisabled(TR("Initialized: %s"), sc.scriptPath.empty() ? "No" : (sc.instance ? "Yes" : "No"));
		}
	}

	void InspectorPanel::drawNativeScriptComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::NativeScriptComponent>("NativeScript", scene, entity)) return;
		auto& nsc = scene->registry().get<FaluEngine::NativeScriptComponent>(entity);

		auto names = FaluEngine::NativeScriptRegistry::get().getRegisteredNames();
		std::string current = nsc.scriptName.empty() ? "(None)" : nsc.scriptName;

		if (ImGui::BeginCombo(TR("Script"), current.c_str()))
		{
			for (auto& name : names)
			{
				bool isSelected = (name == nsc.scriptName);
				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					nsc.bindByName(name);
					nsc.instance = nullptr;
					nsc.initialize = false;
					FaluEngine::SceneManager::get().markDirty();
				}
			}
			ImGui::EndCombo();
		}
	}

	void InspectorPanel::drawLightComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::LightComponent>("Light", scene, entity)) return; 
		{
			auto& lc = scene->registry().get<FaluEngine::LightComponent>(entity);

			const char* types[] = { "Directional","Point","Spot" };
			int type = static_cast<int>(lc.type);
			if (ImGui::Combo(TR("Type"), &type, types, 3))
				lc.type = static_cast<FaluEngine::LightType>(type);

			ImGui::ColorEdit3(TR("Color"), glm::value_ptr(lc.color));
			ImGui::DragFloat(TR("Intensity"), &lc.intensity, 0.01f, 0.0f, 100.0f);

			if (lc.type != FaluEngine::LightType::Directional)
				ImGui::DragFloat(TR("Range"), &lc.range, 0.1f, 0.0f, 1000.0f);

			if (lc.type == FaluEngine::LightType::Spot)
			{
				ImGui::DragFloat(TR("Inner Angle"), &lc.spotInner, 0.5f, 0.0f, 90.0f);
				ImGui::DragFloat(TR("Outer Angle"), &lc.spotOuter, 0.5f, 0.0f, 90.0f);
			}

			ImGui::Checkbox("Enabled", &lc.enable);

			ImGui::Separator();
			ImGui::Text(TR("Shadow"));
			ImGui::Checkbox(TR("Cast Shadow"), &lc.castShadow);
			if (lc.castShadow)
			{
				ImGui::Checkbox(TR("Soft Shadow"), &lc.softShadow);
				ImGui::DragFloat(TR("Bias"), &lc.shadowBias, 0.0001f, 0.0f, 0.1f, "%.4f");
				ImGui::DragFloat(TR("PCF Radius"), &lc.pcfRadius, 0.1f, 0.0f, 5.0f);
			}
		}
	}

	void InspectorPanel::drawSkyComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::SkySphereComponent>("Sky", scene, entity)) return;
		{
			auto& sky = scene->registry().get<FaluEngine::SkySphereComponent>(entity);

			ImGui::Text(TR("Texture"));
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

					const char* payloadPath = static_cast<const char*>(payload->Data);
					const std::string assetPath = FaluEngine::PathResolver::normalizeAssetPath(payloadPath);
					const auto fsPath = FaluEngine::PathResolver::resolve(assetPath);
					if (std::filesystem::exists(fsPath))
					{
						sky.texturePath = assetPath;
						sky.cachedTexture = nullptr;
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Separator();
			ImGui::Text(TR("Gradient (used when no texture)"));
			ImGui::ColorEdit4(TR("Top"), glm::value_ptr(sky.topColor));
			ImGui::ColorEdit4(TR("Horizon"), glm::value_ptr(sky.horizonColor));
			ImGui::ColorEdit4(TR("Bottom"), glm::value_ptr(sky.bottomColor));
			ImGui::DragFloat(TR("Exposure"), &sky.exposure, 0.01f, 0.0f, 10.0f);
			ImGui::Checkbox(TR("Enabled"), &sky.enabled);
		}
	}

	void InspectorPanel::drawAudioComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::AudioSourceComponent>("Audio", scene, entity)) return;

		auto& src = scene->registry().get<FaluEngine::AudioSourceComponent>(entity);
		{
			char buf[512];
			strncpy_s(buf, src.clipPath.c_str(), sizeof(buf));
			if (ImGui::InputText(TR("Clip Path"), buf, sizeof(buf)))
			{
				src.clipPath = buf;
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					const char* payloadPath = static_cast<const char*>(payload->Data);

					const std::string assetPath = FaluEngine::PathResolver::normalizeAssetPath(payloadPath);

					const auto fsPath = FaluEngine::PathResolver::resolve(assetPath);

					if (std::filesystem::exists(fsPath))
					{
						src.clipPath = assetPath;
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SliderFloat(TR("Volume"), &src.volume, 0.0f, 1.0f);
			ImGui::Checkbox(TR("Loop"), &src.loop);
			ImGui::Checkbox(TR("Play On Awake"), &src.playOnAwake);

			if (ImGui::Button(TR("Play (Preview)")))
			{
				FaluEngine::AudioEngine::get().play(
					src.clipPath, src.volume, false
				);
			}

			bool audioExists = !src.clipPath.empty() && std::filesystem::exists(
				FaluEngine::PathResolver::resolve(src.clipPath)
			);
			if (!src.clipPath.empty() && !audioExists)
				ImGui::TextColored({ 1.0f,0.3f,0.3f,1.0f }, "File not Found");
		}

	}

	//===== UI ========

	void InspectorPanel::drawRectTransformComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!scene->registry().all_of<FaluEngine::RectTransformComponent>(entity)) return;

		auto& rt = scene->registry().get<FaluEngine::RectTransformComponent>(entity);
		if (ImGui::CollapsingHeader("Rect Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat2(TR("Anchor Min"), &rt.anchorMin.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat2(TR("Anchor Max"), &rt.anchorMax.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat2(TR("Anchored Position"), &rt.anchoredPos.x, 1.0f);
			ImGui::DragFloat2(TR("Size Delta"), &rt.sizeDelta.x, 1.0f, 0.0f, 8192.0f);
			ImGui::DragFloat2(TR("Pivot"), &rt.pivot.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat(TR("Rotation"), &rt.rotation, 0.5f);
			ImGui::DragFloat2(TR("Scale"), &rt.scale.x, 0.01f);

			ImGui::Separator();
			ImGui::TextDisabled(TR("Computed Position: (%.1f, %.1f)"),
				rt.computedPosition.x, rt.computedPosition.y);
			ImGui::TextDisabled(TR("Computed Size: (%.1f, %.1f)"),
				rt.computedSize.x, rt.computedSize.y);
		}
	}

	void InspectorPanel::drawCanvasComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::CanvasComponent>("Canvas", scene, entity)) return;
		auto& canvas = scene->registry().get<FaluEngine::CanvasComponent>(entity);
		{
			const char* modes[] = { "Screen Space - Overlay","World Space" };
			int currentMode = static_cast<int>(canvas.renderMode);
			if (ImGui::Combo(TR("Render Mode"), &currentMode, modes, 2))
			{
				canvas.renderMode = static_cast<FaluEngine::CanvasRenderMode>(currentMode);
			}

			if (canvas.renderMode == FaluEngine::CanvasRenderMode::WorldSpace)
			{
				ImGui::TextColored({ 1.0f,0.7f,0.2f,1.0f }, "World Space is not implemented yet.");// <--未実装メッセージ
			}

			ImGui::DragFloat2(TR("Reference Resolution"), &canvas.referenceResolution.x, 1.0f, 1.0f, 8192.0f);
			ImGui::DragInt(TR("Sort Order"), &canvas.sortOrder);
			ImGui::Checkbox(TR("Enabled"), &canvas.enabled);
		}
	}

	void InspectorPanel::drawImageComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::ImageComponent>("Image", scene, entity)) return;

		auto& img = scene->registry().get<FaluEngine::ImageComponent>(entity);
		{
			char buf[512];
			strncpy_s(buf, img.texturePath.c_str(), sizeof(buf));
			if (ImGui::InputText(TR("Texture Path"), buf, sizeof(buf)))
			{
				img.texturePath = buf;
				img.cachedTexture = nullptr;
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					const char* payloadPath = static_cast<const char*>(payload->Data);

					const std::string assetPath = FaluEngine::PathResolver::normalizeAssetPath(payloadPath);

					const auto fsPath = FaluEngine::PathResolver::resolve(assetPath);

					if (std::filesystem::exists(fsPath))
					{
						img.texturePath = assetPath;
						img.cachedTexture = nullptr;
					}
				}
				ImGui::EndDragDropTarget();
			}

			bool fileExists = !img.texturePath.empty() &&
				std::filesystem::exists(
					FaluEngine::PathResolver::resolve(img.texturePath));
			if (!img.texturePath.empty() && !fileExists)
			{
				ImGui::TextColored({ 1.0f,0.3f,0.3f,1.0f }, "File not found");
			}

			ImGui::ColorEdit4(TR("Color"), &img.color.x);
			ImGui::Checkbox(TR("Visible"), &img.visible);
		}
	}

	void InspectorPanel::drawButtonComponent(FaluEngine::Scene* scene, entt::entity entity)
	{
		if (!drawComponentHeader<FaluEngine::ButtonComponent>("Button", scene, entity)) return;

		auto& btn = scene->registry().get<FaluEngine::ButtonComponent>(entity);
		{
			ImGui::Checkbox(TR("Interactable"), &btn.interactable);
			ImGui::ColorEdit4(TR("Normal Color"), &btn.normalColor.x);
			ImGui::ColorEdit4(TR("Hovered Color"), &btn.hoveredColor.x);
			ImGui::ColorEdit4(TR("Pressed Color"), &btn.pressedColor.x);

			ImGui::Separator();
			ImGui::TextDisabled(TR("Hovered: %s"), btn.isHovered ? "true" : "false");
			ImGui::TextDisabled(TR("Pressed: %s"), btn.isPressed ? "true" : "false");
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
			ImGui::Text(TR("Albedo Color"));
			if (ImGui::ColorEdit4("##Albedo", glm::value_ptr(material->albedoColor)))
				changed = true;

			ImGui::Text(TR("Albedo Map"));
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
			if (ImGui::SliderFloat(TR("Metallic"), &material->metallic, 0.0f, 1.0f)) changed = true;
			if (ImGui::SliderFloat(TR("Roughness"), &material->roughness, 0.0f, 1.0f)) changed = true;

			ImGui::Text(TR("Metallic/Roughness Map (R=Metal,G=Rough)"));
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
			ImGui::Text(TR("Normal Map"));
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
			ImGui::Text(TR("AO Map"));
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
			if (ImGui::ColorEdit3(TR("Emissive Color"), glm::value_ptr(material->emissiveColor)))
				changed = true;
			if (ImGui::DragFloat(TR("Emissive Strength"), &material->emissiveStrength, 0.01f, 0.0f, 50.0f))
				changed = true;

			ImGui::Text(TR("Emissive Map"));
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
			ImGui::Text(TR("Vertex Shader (optional)"));
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

			ImGui::Text(TR("Pixel Shader (optional)"));
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
			if (ImGui::Button(TR("Save Material"), { -1,0 }))
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

	void InspectorPanel::drawAddComponentMenu(FaluEngine::Scene* scene, entt::entity entity, ID3D11Device* device)
	{
		if (!ImGui::BeginPopup("AddComponent")) return;

		FaluEngine::Entity e(entity, scene);


		//====== Rendering ======
		if (ImGui::BeginMenu(TR("Rendering")))
		{
			if(!scene->registry().all_of<FaluEngine::MeshComponent>(entity))
			{ 
				if (ImGui::MenuItem(TR("Mesh Component")))
				{
					if (e.hasComponent<FaluEngine::CanvasComponent>() ||
						e.hasComponent<FaluEngine::RectTransformComponent>())
					{
						LOG_WARN("MeshComponent should not be added to a UI entity!");
					}
					else
					{
						e.addComponent<FaluEngine::MeshComponent>();
						FaluEngine::SceneManager::get().markDirty();
					}
				}
			}

			if (!scene->registry().all_of<FaluEngine::CameraComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Camera Component")))
				{
					e.addComponent<FaluEngine::CameraComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			if (!scene->registry().all_of<FaluEngine::LightComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Light Component")))
				{
					e.addComponent<FaluEngine::LightComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			if (!scene->registry().all_of<FaluEngine::SkySphereComponent>(entity))
			{
				if (ImGui::MenuItem(TR("SkySphere Component")))
				{
					e.addComponent<FaluEngine::SkySphereComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			if (!scene->registry().all_of<FaluEngine::AnimatorComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Animator Component")))
				{
					e.addComponent<FaluEngine::AnimatorComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			ImGui::EndMenu();
		}


		//==== UI =======
		if (ImGui::BeginMenu("UI"))
		{
			if (!scene->registry().all_of<FaluEngine::CanvasComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Canvas Component")))
				{
					e.addComponent<FaluEngine::CanvasComponent>();
					if (!e.hasComponent<FaluEngine::RectTransformComponent>())
					{
						e.addComponent<FaluEngine::RectTransformComponent>();
						FaluEngine::SceneManager::get().markDirty();
					}
				}
			}

			if (!scene->registry().all_of<FaluEngine::ImageComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Image Component")))
				{
					e.addComponent<FaluEngine::ImageComponent>();
					if (!e.hasComponent<FaluEngine::RectTransformComponent>())
					{
						e.addComponent<FaluEngine::RectTransformComponent>();
						FaluEngine::SceneManager::get().markDirty();
					}

				}
			}

			if (!scene->registry().all_of<FaluEngine::ButtonComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Button Component")))
				{
					e.addComponent<FaluEngine::ButtonComponent>();
					if (!e.hasComponent<FaluEngine::RectTransformComponent>())
					{
						e.addComponent<FaluEngine::RectTransformComponent>();
						FaluEngine::SceneManager::get().markDirty();
					}
				}
			}

			ImGui::EndMenu();
		}



		//====== Physics =====
		if (ImGui::BeginMenu(TR("Physics")))
		{
			if (!scene->registry().all_of<FaluEngine::RigidbodyComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Rigidbody Component")))
				{
					e.addComponent<FaluEngine::RigidbodyComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}


			ImGui::EndMenu();
		}

		//===== Terrain =====
		if (ImGui::BeginMenu(TR("Terrain")))
		{
			if (!scene->registry().all_of<FaluEngine::TerrainComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Terrain Field Component")))
				{
					FaluEngine::addTerrainComponents(*scene, e, FaluEngine::TerrainComponent{}, device);
					FaluEngine::SceneManager::get().markDirty();
				}

			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(TR("Scripting")))
		{
			if (!scene->registry().all_of<FaluEngine::ScriptComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Script(lua) Component")))
				{
					e.addComponent<FaluEngine::ScriptComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			if (!scene->registry().all_of<FaluEngine::NativeScriptComponent>(entity))
			{
				if (ImGui::MenuItem(TR("Script(C++) Component")))
				{
					e.addComponent<FaluEngine::NativeScriptComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			ImGui::EndMenu();
		}


		//====== Audio =======
		if (ImGui::BeginMenu(TR("Audio")))
		{
			if (!scene->registry().all_of<FaluEngine::AudioSourceComponent>(entity))
			{
				if (ImGui::MenuItem(TR("AudioSource Component")))
				{
					e.addComponent<FaluEngine::AudioSourceComponent>();
					FaluEngine::SceneManager::get().markDirty();
				}
			}

			ImGui::EndMenu();
		}
		
		ImGui::EndPopup();
	}
}
