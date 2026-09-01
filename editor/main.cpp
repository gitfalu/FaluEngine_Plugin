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
#include "core/EditorStateManager.h"
#include "core/InputManager.h"
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
#include "panels/GameViewPanel.h"
#include "platform/Window.h"
#include "plugin/PluginManager.h"
#include <imgui.h>
#include <ImGuizmo.h>

class EditorScene : public FaluEngine::Scene
{
public:
    EditorScene() : Scene("EditorScene") {}

    void onEnter() override
    {
        auto cam = createEntity("Camera");
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

        auto sky = createEntity("Sky");
        auto& skyComp = sky.addComponent<FaluEngine::SkySphereComponent>();
        skyComp.topColor = { 0.1f,0.3f,0.8f,1.0f };
        skyComp.horizonColor = { 0.6f,0.75f,0.9f,1.0f };
        skyComp.bottomColor = { 0.2f,0.15f,0.1f,1.0f };

        auto character = createEntity("Character");
        auto& cMesh = character.addComponent<FaluEngine::MeshComponent>();
        cMesh.meshPath = FaluEngine::PathResolver::resolveStr("assets/meshes/Laughing.fbx");

        auto& animator = character.addComponent<FaluEngine::AnimatorComponent>();
        animator.currentClipName = "mixamo.com";
        animator.playing = true;
        animator.loop = true;

        auto canvas = createEntity("Canvas");
        canvas.addComponent<FaluEngine::CanvasComponent>();

        auto panel = createEntity("Panel");
        auto& panelRT = panel.addComponent<FaluEngine::RectTransformComponent>();
        panelRT.sizeDelta = { 300.0f,100.0f };
        auto& panelImg = panel.addComponent<FaluEngine::ImageComponent>();
        panelImg.color = { 1.0f,0.0f,0.0f,0.8f };

        panel.setParent(canvas);

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
        getSceneManager().scanSceneFolder(
            std::filesystem::path(
                FaluEngine::PathResolver::resolveStr("assets/scenes")));

        if (getSceneManager().getSceneNames().empty()) {
            getSceneManager().registerScene<EditorScene>("editor");
        }

        auto names = getSceneManager().getSceneNames();
        if (!names.empty()) {
            getSceneManager().switchTo(names[0]);

            std::string path = getSceneManager().getScenePath(names[0]);
            if (!path.empty()) {
                FaluEngine::SceneSerializer serializer(
                    *getSceneManager().getActive());
                serializer.deserialize(path);
            }
        }

        getSceneManager().setUpdateGate([]() {
            return FaluEngine::EditorStateManager::get().isPlaying();
            });

        m_editorCamera.setPerspective(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        m_editorCamera.setPosition({ 0.0f,3.0f,-5.0f });
        m_cameraCtrl = std::make_unique<FaluEngine::CameraController>(m_editorCamera);

        m_contentBrowser.init(
            std::filesystem::path(FaluEngine::PathResolver::resolveStr("assets")));

        FaluEngine::EditorStateManager::get().setOnBeforeStop([this]() {
            m_hierarchy.clearSelected();
            });

        FaluEngine::EventBus::get().subscribe<FaluEngine::MouseMovedEvent>(
            [this](const FaluEngine::MouseMovedEvent& e) {
                if (!m_cameraCtrl) return;
                if (m_sceneView.isFocused())
                    m_cameraCtrl->onMouseMove(e.x, e.y);
                else
                    m_cameraCtrl->resetMouseState();
        });

        FaluEngine::EventBus::get().subscribe<FaluEngine::MouseButtonPressedEvent>(
            [this](const FaluEngine::MouseButtonPressedEvent& e) {
                if (m_cameraCtrl && m_sceneView.isFocused())
                    m_cameraCtrl->onMouseButtonDown(
                        static_cast<int>(e.button));
            });

        FaluEngine::EventBus::get().subscribe<FaluEngine::MouseButtonReleasedEvent>(
            [this](const FaluEngine::MouseButtonReleasedEvent& e) {
                if (m_cameraCtrl && m_sceneView.isFocused())
                    m_cameraCtrl->onMouseButtonUp(
                        static_cast<int>(e.button));
            });

        FaluEngine::EventBus::get().subscribe<FaluEngine::MouseScrolledEvent>(
            [this](const FaluEngine::MouseScrolledEvent& e) {
                if (m_cameraCtrl && m_sceneView.isFocused())
                    m_cameraCtrl->onMouseScroll(e.offsetY);
            });

        LOG_INFO("FaluEngine Editor started");
    }
    void onUpdate(float deltaTime) override 
    { 
        m_fps = 1.0f / (deltaTime > 0.0f ? deltaTime : 1.0f);

        if (m_cameraCtrl && m_sceneView.isFocused())
            m_cameraCtrl->onUpdate(deltaTime);

        // F key focus
        auto& input = FaluEngine::InputManager::get();
        if (input.isKeyPressed(FaluEngine::Key::F) &&
            m_sceneView.isFocused() &&
            m_hierarchy.getSelected() != entt::null)
        {
            auto* scene = getSceneManager().getActive();
            if (scene && scene->registry()
                .all_of<FaluEngine::TransformComponent>(
                    m_hierarchy.getSelected())) {
                auto& t = scene->registry().get<FaluEngine::TransformComponent>(
                    m_hierarchy.getSelected());
                if (m_cameraCtrl)
                    m_cameraCtrl->focusOn(t.worldMatrix[3]);
            }
        }

        // Ctrl+S
        if (input.isKeyDown(FaluEngine::Key::Control) &&
            input.isKeyPressed(FaluEngine::Key::S) &&
            FaluEngine::EditorStateManager::get().isEditing())
        {
            m_requestSave = true;
        }

        // Gizmo Shortcut
        if (input.isKeyPressed(FaluEngine::Key::W)) m_sceneView.setMode(Editor::GizmoMode::Translate);
        if (input.isKeyPressed(FaluEngine::Key::E)) m_sceneView.setMode(Editor::GizmoMode::Rotate);
        if (input.isKeyPressed(FaluEngine::Key::R)) m_sceneView.setMode(Editor::GizmoMode::Scale);
    }
    void onRender() override 
    {
        auto* renderer = static_cast<FaluEngine::DX11Renderer*>(getRenderer());
        auto* scene = getSceneManager().getActive();

        std::wstring title = L"FaluEngine Editor";
        if (scene)
        {
            std::string sceneName = scene->getName();
            title += L" - " + std::wstring(sceneName.begin(), sceneName.end());
            if (getSceneManager().isDirty()) title += L" *";
        }
        m_window->setTitle(title);

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                //====== Script Reload ======
                if (ImGui::MenuItem("Reload Scripts"))
                {
                    auto view = scene->registry().view<FaluEngine::NativeScriptComponent>();
                    for (auto entity : view)
                    {
                        auto& nsc = view.get<FaluEngine::NativeScriptComponent>(entity);
                        nsc.instance = nullptr;
                        nsc.initialize = false;
                    }

                    std::string gameCodePath =
                        FaluEngine::PathResolver::resolveStr("GameCode.dll");

                    if (FaluEngine::PluginManager::get().reload(gameCodePath))
                        LOG_INFO("Scripts reloaded");
                    else
                        LOG_ERROR("Failed to reload GameCode.dll");
                }

                if (ImGui::MenuItem("New"))
                {
                    if (getSceneManager().isDirty())
                        m_openUnsavedWarningPopup = true;
                    else
                        m_openNewScenePopup = true;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Save Scene", "Ctrl + S",false,
                    FaluEngine::EditorStateManager::get().isEditing()))
                {
                    m_requestSave = true;
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) quit();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Scene")) {
                if (ImGui::MenuItem("Create Entity") && scene)
                    scene->createEntity("New Entity");

                ImGui::Separator();
                ImGui::TextDisabled("Switch Scene");

                for (auto& name : getSceneManager().getSceneNames()) {
                    bool isActive = scene && scene->getName() == name;
                    if (ImGui::MenuItem(name.c_str(), nullptr, isActive)) {
                        getSceneManager().switchTo(name);
                        m_hierarchy.clearSelected();

                        std::string path = getSceneManager().getScenePath(name);
                        if (!path.empty())
                        {
                            FaluEngine::SceneSerializer serializer(
                                *getSceneManager().getActive());
                            serializer.deserialize(path);
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::SameLine();

        {
            auto& stateManager = FaluEngine::EditorStateManager::get();
            bool isEditing = stateManager.isEditing();
            bool isPlaying = stateManager.isPlaying();
            bool isPaused = stateManager.isPaused();

            ImGui::Begin("##Toolbar", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
            );

            if (isEditing)
            {
                if (ImGui::Button("Play"))
                {
                    auto* activeScene = getSceneManager().getActive();
                    if (activeScene) stateManager.play(*activeScene);
                }
            }
            else
            {
                ImGui::BeginDisabled(true);
                ImGui::Button("Play");
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            if (isPlaying)
            {
                if (ImGui::Button("Pause")) stateManager.pause();
            }
            else if(isPaused)
            {
                if (ImGui::Button("Resume"))stateManager.resume();
            }
            else
            {
                ImGui::BeginDisabled(true);
                ImGui::Button("Pause");
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            if (!isEditing)
            {
                if (ImGui::Button("Stop"))
                {
                    auto* activeScene = getSceneManager().getActive();
                    if (activeScene) stateManager.stop(*activeScene);
                }
            }
            else
            {
                ImGui::BeginDisabled(true);
                ImGui::Button("Stop");
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            ImGui::TextColored(
                isPlaying ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) :
                isPaused ? ImVec4(1.0f, 1.0f, 0.4f, 1.0f) :
                ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                " [%s]", isPlaying ? "PLAYING" : isPaused ? "PAUSED" : "EDITING"
            );
            ImGui::End();
        }

        if (m_openNewScenePopup)
        {
            ImGui::OpenPopup("NewSceneName");
            {
                m_openNewScenePopup = false;
            }
        }

        if (ImGui::BeginPopup("NewSceneName"))
        {
            static char nameBuf[128] = "NewScene";
            ImGui::InputText("Scene Name", nameBuf, sizeof(nameBuf));
            if (ImGui::Button("Create"))
            {
                m_hierarchy.clearSelected();
                getSceneManager().createNewScene(nameBuf);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (m_openUnsavedWarningPopup) { ImGui::OpenPopup("Unsaved Changes"); m_openUnsavedWarningPopup = false; }
        if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("You have unsaved changes. Continue without saving?");
            if (ImGui::Button("Save & Continue")) { saveCurrentScene(); m_openNewScenePopup = true; ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Discard")) { m_openNewScenePopup = true; ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
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
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f,0.0f });
        ImGui::Begin("DockSpace", nullptr, dockFlags);
        ImGui::PopStyleVar();

        ImGuiID dockId = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockId, { 0.0f,0.0f });
        ImGui::End();
        // Hierarchy Render
        m_hierarchy.draw(scene);
        // Inspector Render
        m_inspector.draw(scene, m_hierarchy.getSelected());

        m_sceneView.beginFrame();

        if (m_cameraCtrl)
        {
            m_cameraCtrl->setViewRect(
                m_sceneView.getWindowPosX(),
                m_sceneView.getWindowPosY(),
                m_sceneView.getWidth(),
                m_sceneView.getHeight()
            );
        }

        if (renderer && scene)
        {// SceneRender
            uint32_t w = static_cast<uint32_t>(m_sceneView.getWidth());
            uint32_t h = static_cast<uint32_t>(m_sceneView.getHeight());
            if (w < 1) w = 1;
            if (h < 1) h = 1;

            float aspect = static_cast<float>(w) / static_cast<float>(h);
            m_editorCamera.setPerspective(
                m_editorCamera.getFovDeg(), aspect,
                m_editorCamera.getNearClip(), m_editorCamera.getFarClip());

            renderer->beginOffscreen(w, h);
            renderer->setViewProjection(
                m_editorCamera.getView(), m_editorCamera.getProjection());
            scene->onRender(false);
            renderer->endOffscreen();
        }
        // SceneViewRender
        m_sceneView.drawImage(renderer);
        m_sceneView.drawGizmo(
            scene, m_hierarchy.getSelected(),renderer);
        m_sceneView.endFrame();

        // GameView
        m_gameView.beginFrame();

        if (renderer && scene)
        {
            uint32_t gw = static_cast<uint32_t>(m_gameView.getWidth());
            uint32_t gh = static_cast<uint32_t>(m_gameView.getHeight());

            if (gw < 1) gw = 1;
            if (gh < 1) gh = 1;

            renderer->setUILocalMousePos(m_gameView.getLocalMousePos());
            renderer->beginGameOffscreen(gw, gh);
            scene->onRender();
            renderer->endGameOffscreen();
        }
        m_gameView.drawImage(renderer);
        
        if (m_contentBrowser.draw(scene, m_hierarchy.getSelected()))
            m_hierarchy.clearSelected();

        ImGui::Begin("Stats");
        ImGui::Text("FPS: %.1f (%.3f ms)", m_fps, 1000.0f / (m_fps > 0 ? m_fps : 1));
        if (scene)
        {
            ImGui::Text("Entities: %u", scene->entityCount());
            ImGui::Text("Scene: %s", scene->getName().c_str());
        }
        ImGui::End();

        saveCurrentScene();
    }
    void onShutdown() override 
    {
        LOG_INFO("FaluEngine Editor shutdown");
    }

private:
    void saveCurrentScene()
    {
        if (!m_requestSave) return;

        auto* scene = getSceneManager().getActive();
        if (scene) {
            FaluEngine::SceneSerializer serializer(*scene);
            std::string savePath = getSceneManager().getScenePath(scene->getName());
            if (savePath.empty())
            {
                savePath = FaluEngine::PathResolver::resolveStr(
                    "assets/scenes/" + scene->getName() + ".scene");
                getSceneManager().setScenePath(scene->getName(), savePath);
            }
            if (serializer.serialize(savePath))
            {
                FALU_ENGINE_LOG_INFO("Scene saved: {}", savePath);
                getSceneManager().clearDirty();
                m_requestSave = false;
            }

        }
    }


private:
    float m_fps = 0.0f;
    std::unique_ptr<FaluEngine::CameraController> m_cameraCtrl;
    Editor::HierarchyPanel m_hierarchy;
    Editor::InspectorPanel m_inspector;
    Editor::SceneViewPanel m_sceneView;
    Editor::ContentBrowserPanel m_contentBrowser;
    Editor::GameViewPanel m_gameView;
    FaluEngine::Camera m_editorCamera;

    bool m_openNewScenePopup = false;
    bool m_openUnsavedWarningPopup = false;
    bool m_requestSave = false;

};

int main()
{
    // リークチェック
#if defined(_DEBUG)
#include <crtdbg.h>
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    EditorApp editor;
    return editor.run();
}
