#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "core/Application.h"
#include "core/Logger.h"
#include "core/PathResolver.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"
#include "scene/SceneSerializer.h"
#include "physics/PhysicsSystem.h"
#include "physics/RigidbodyComponent.h"
#include "renderer/dx11/DX11Renderer.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/SceneViewPanel.h"
#include "panels/ContentBrowserPanel.h"
#include "core/InputManager.h"
#include <imgui.h>
#include <ImGuizmo.h>

class EditorScene : public FaluEngine::Scene
{
public:
    EditorScene() : Scene("EditorScene") {}

    void onEnter() override
    {
        auto cam = createEntity("EditorCamera");
        auto& camComp = cam.addComponent<FaluEngine::CameraComponent>();
        camComp.isPrimary = true;
        camComp.camera.setPosition({ 0.0f,3.0f,-5.0f });

        // Ground
        auto ground = createEntity("Ground");
        auto& gTransform = ground.getComponent<FaluEngine::TransformComponent>();
        gTransform.position = { 0.0f,-1.0f,0.0f };
        auto& gMesh = ground.addComponent<FaluEngine::MeshComponent>();
        gMesh.meshPath = FaluEngine::PathResolver::resolveStr("assets/meshes/plane.obj");
        auto& gRb = ground.addComponent<FaluEngine::RigidbodyComponent>();
        gRb.bodyType = FaluEngine::BodyType::Static;
        gRb.halfExtents = { 10.0f,0.1f,10.0f };

        // Box
        auto box = createEntity("Box");
        auto& bTransform = box.getComponent<FaluEngine::TransformComponent>();
        bTransform.position = { 0.0f,3.0f,0.0f };
        auto& bMesh = box.addComponent<FaluEngine::MeshComponent>();
        bMesh.meshPath = FaluEngine::PathResolver::resolveStr("assets/meshes/box.obj");
        bMesh.texturePath = FaluEngine::PathResolver::resolveStr("assets/textures/box.png");
        auto& bRb = box.addComponent<FaluEngine::RigidbodyComponent>();
        bRb.bodyType = FaluEngine::BodyType::Dynamic;
        bRb.halfExtents = { 0.5f,0.5f,0.5f };

        auto light = createEntity("DirectionalLight");
        light.getComponent<FaluEngine::TransformComponent>().position = { 0.0f,5.0f,0.0f };
        auto& lc = light.addComponent<FaluEngine::LightComponent>();
        lc.type = FaluEngine::LightType::Directional;
        lc.color = { 1.0f,1.0f,0.0f };
        lc.intensity = 1.0f;
        lc.castShadow = true;
        lc.softShadow = true;

        FaluEngine::PhysicsSystem::get().registerScene(*this);
        FALU_ENGINE_LOG_INFO("EditorScene entered - {} entities", entityCount());
    }

private:

};

class EditorApp : public FaluEngine::Application {
public:
    EditorApp() : Application({ .title = L"FaluEngine Editor", .width = 1280, .height = 720 }) {}
    void onInit()                  override 
    {
        getSceneManager().registerScene<EditorScene>("editor");
        getSceneManager().switchTo("editor");

        auto* scene = getSceneManager().getActive();
        scene->registry()
            .view<FaluEngine::CameraComponent>()
            .each([&](auto, FaluEngine::CameraComponent& c) {
            if (!m_cameraCtrl)
                m_cameraCtrl = std::make_unique<FaluEngine::CameraController>(c.camera);
                });

        m_contentBrowser.init(
            std::filesystem::path(FaluEngine::PathResolver::resolveStr("assets")));

        LOG_INFO("FaluEngine Editor started");
    }
    void onUpdate(float deltaTime) override 
    { 
        m_fps = 1.0f / (deltaTime > 0.0f ? deltaTime : 1.0f);
        if (m_cameraCtrl && m_sceneView.isFocused())
            m_cameraCtrl->onUpdate(deltaTime);

        // Gizmo Shortcut
        auto& input = FaluEngine::InputManager::get();
        if (input.isKeyPressed(FaluEngine::Key::W)) m_sceneView.setMode(Editor::GizmoMode::Translate);
        if (input.isKeyPressed(FaluEngine::Key::E)) m_sceneView.setMode(Editor::GizmoMode::Rotate);
        if (input.isKeyPressed(FaluEngine::Key::R)) m_sceneView.setMode(Editor::GizmoMode::Scale);
    }
    void onRender()                override 
    {
        auto* renderer = static_cast<FaluEngine::DX11Renderer*>(getRenderer());
        auto* scene = getSceneManager().getActive();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene", "Ctrl + S"))
                {
                    auto* scene = getSceneManager().getActive();
                    if (scene) {
                        FaluEngine::SceneSerializr serializer(*scene);
                        std::string savePath = FaluEngine::PathResolver::resolveStr(
                            "assets/scenes/" + scene->getName() + ".scene");
                        if (serializer.serialize(savePath))
                            FALU_ENGINE_LOG_INFO("Scene saved: {}", savePath);
                    }
                }
                if (ImGui::MenuItem("Load Scene", "Ctrl + O")) {
                    auto* scene = getSceneManager().getActive();
                    if (scene) {
                        FaluEngine::SceneSerializr serializer(*scene);
                        std::string loadPath = FaluEngine::PathResolver::resolveStr(
                            "assets/scenes/" + scene->getName() + ".scene");
                        if (serializer.deserialize(loadPath))
                        {
                            FaluEngine::PhysicsSystem::get().unregisterScene(*scene);
                            FaluEngine::PhysicsSystem::get().registerScene(*scene);
                            FALU_ENGINE_LOG_INFO("Scene loaded: {}", loadPath);
                        }
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Exit"))quit();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scene"))
            {
                if (ImGui::MenuItem("Create Entity") && scene)
                    scene->createEntity("New Entity");
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags dockFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
        ImGui::Begin("DockSpace", nullptr, dockFlags);
        ImGui::PopStyleVar();

        ImGuiID dockId = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockId, { 0.0f,0.0f }, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
        // Hierarchy Render
        m_hierarchy.draw(scene);
        // Inspector Render
        m_inspector.draw(scene, m_hierarchy.getSelected());

        m_sceneView.beginFrame();

        if (renderer && scene)
        {// SceneRender
            uint32_t w = static_cast<uint32_t>(m_sceneView.getWidth());
            uint32_t h = static_cast<uint32_t>(m_sceneView.getHeight());
            if (w < 1) w = 1;
            if (h < 1) h = 1;

            renderer->beginOffscreen(w, h);
            scene->onRender();
            renderer->endOffscreen();
        }
        // SceneViewRender
        m_sceneView.drawImage(renderer);
        m_sceneView.drawGizmo(scene, m_hierarchy.getSelected());

        m_sceneView.endFrame();

        m_contentBrowser.draw(scene, m_hierarchy.getSelected());

        ImGui::Begin("Stats");
        ImGui::Text("FPS: %.1f (%.3f ms)", m_fps, 1000.0f / (m_fps > 0 ? m_fps : 1));
        if (scene)
        {
            ImGui::Text("Entities: %u", scene->entityCount());
            ImGui::Text("Scene: %s", scene->getName().c_str());
        }
        ImGui::End();
    }
    void onShutdown() override 
    {
        LOG_INFO("FaluEngine Editor shutdown");
    }

private:
    float m_fps = 0.0f;
    std::unique_ptr<FaluEngine::CameraController> m_cameraCtrl;
    Editor::HierarchyPanel m_hierarchy;
    Editor::InspectorPanel m_inspector;
    Editor::SceneViewPanel m_sceneView;
    Editor::ContentBrowserPanel m_contentBrowser;

};

int main()
{
    EditorApp editor;
    return editor.run();
}
