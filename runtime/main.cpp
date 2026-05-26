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
#include "core/InputManager.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"
#include "renderer/dx11/DX11Renderer.h"
#include "asset/loaders/MeshLoader.h"
#include "physics/RigidbodyComponent.h"
#include "physics/PhysicsSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

class GameScene : public FaluEngine::Scene {
public:
    GameScene() : Scene("GameScene") {}

    void onEnter() override {
        // カメラ Entity
        auto camEntity = createEntity("MainCamera");
        auto& camComp = camEntity.addComponent<FaluEngine::CameraComponent>();
        camComp.isPrimary = true;
        camComp.camera.setPosition({ 0.f, 0.f, -3.f });

        auto ground = createEntity("Ground");
        auto& groundRb = ground.addComponent<FaluEngine::RigidbodyComponent>();
        groundRb.bodyType = FaluEngine::BodyType::Static;
        groundRb.shape = FaluEngine::ColliderShape::Box;
        groundRb.halfExtents = { 10.0f,0.0f,10.0f };
        ground.getComponent<FaluEngine::TransformComponent>().position = { 0.0f,-2.0f,0.0f };

        auto meshEntity = createEntity("Cube");
        auto& meshComp = meshEntity.addComponent<FaluEngine::MeshComponent>();
        auto& boxRb = meshEntity.addComponent<FaluEngine::RigidbodyComponent>();
        boxRb.bodyType = FaluEngine::BodyType::Dynamic;
        boxRb.shape = FaluEngine::ColliderShape::Box;
        boxRb.halfExtents = { 0.5f,0.5f,0.5f };
        meshEntity.getComponent<FaluEngine::TransformComponent>().position = { 0.0f,3.0f,0.0f };

        meshComp.meshPath = FaluEngine::PathResolver::resolveStr("assets/meshes/box.obj");
        meshComp.cachedMesh = FaluEngine::AssetManager::get()
            .load<FaluEngine::MeshAsset>(meshComp.meshPath);
        meshComp.texturePath = FaluEngine::PathResolver::resolveStr("assets/textures/box.png");
        
        // 物理ワールドに登録
        FaluEngine::PhysicsSystem::get().registerScene(*this);

        auto& scriptComp = meshEntity.addComponent<FaluEngine::ScriptComponent>();
        scriptComp.scriptPath = FaluEngine::PathResolver::resolveStr("assets/scripts/test.lua");

        FALU_ENGINE_LOG_INFO("GameScene entered — {} entities", entityCount());
    }

};

class EditorApp : public FaluEngine::Application {
public:
    EditorApp() : Application({.title = L"FaluEngine Editor",.width = 1280,.height = 720}) {}

    void onInit() override {
        getSceneManager().registerScene<GameScene>("game");
        getSceneManager().switchTo("game");

        auto* scene = getSceneManager().getActive();
        auto view = scene->registry()
            .view<FaluEngine::CameraComponent>();
        for (auto entity : view) {
            auto& c = view.get<FaluEngine::CameraComponent>(entity);

            m_cameraCtrl = std::make_unique<FaluEngine::CameraController>(c.camera);
            break;
        }

        LOG_INFO("FaluEngine Editor started");
    }

    void onUpdate(float deltaTime) override
    {
        m_fps = 1.0f / (deltaTime > 0.0f ? deltaTime : 1.0f);

        if (m_cameraCtrl) m_cameraCtrl->onUpdate(deltaTime);
        m_rotation += 90.f * deltaTime;

        auto& input = FaluEngine::InputManager::get();
        if (input.isKeyReleased(FaluEngine::Key::Space))
            FALU_ENGINE_LOG_INFO("Space pressed!");
        if (input.isMouseButtonPressed(FaluEngine::MouseButton::Left))
            FALU_ENGINE_LOG_INFO("Left click at ({:.0f},{:.0f})",
                input.getMousePosition().x, input.getMousePosition().y);
    }

    void onRender() override
    {
        ImGui::Begin("FaluEngine");
        ImGui::Text("FPS: %.1f", m_fps);
        ImGui::Separator();
        ImGui::Text("Resolution: %d * %d", getConfig().width, getConfig().height);
        ImGui::ColorEdit4("Clear Color", m_clearColor);
        ImGui::End();

    }

    void onShutdown() override
    {
        LOG_INFO("FaluEngine Editor shutdown");
    }

private:
    float m_fps = 0.0f;
    float m_clearColor[4] = { 0.18f,0.18f,0.2f,1.0f };

    float m_rotation = 0.0f;
    std::unique_ptr<FaluEngine::CameraController> m_cameraCtrl;
};

int main() {
    EditorApp app;
    return app.run();
}
