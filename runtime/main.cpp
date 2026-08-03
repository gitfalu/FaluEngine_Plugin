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
#include "scene/SceneSerializer.h"
#include "renderer/dx11/DX11Renderer.h"
#include "asset/loaders/MeshLoader.h"
#include "physics/RigidbodyComponent.h"
#include "physics/PhysicsSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#ifdef ENGINE_DEBUG
#include <imgui.h>
#endif

static constexpr const char* kStartupScenePath = "assets/scenes/main.scene";

class GameScene : public FaluEngine::Scene {
public:
    GameScene() : Scene("GameScene") {}

    void onEnter() override {
        std::string scenePath = FaluEngine::PathResolver::resolveStr(kStartupScenePath);

        FaluEngine::SceneSerializer serializer(*this);
        bool loaded = std::filesystem::exists(scenePath) && serializer.deserialize(scenePath);

        if (!loaded)
        {
            LOG_ERROR("Failed to loaded startup scene: '{}'. Falling back to a minimal empty scene.", scenePath);

            // fallback when SceneFile is not found / broken(continue startup to ready only camera)
            auto camEntity = createEntity("MainCamera");
            auto& camComp = camEntity.addComponent<FaluEngine::CameraComponent>();
            camComp.isPrimary = true;
            camComp.camera.setPosition({ 0.0f,0.0f,-3.0f });
        }

        FaluEngine::PhysicsSystem::get().registerScene(*this);

        FALU_ENGINE_LOG_INFO("GameScne entered ('{}') - {} entities", loaded ? scenePath : "fallback", entityCount());
    }

};

class RuntimeApp : public FaluEngine::Application {
public:
    RuntimeApp() : Application({.title = L"FaluEngine",.width = 1280,.height = 720}) {}

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

        LOG_INFO("FaluEngine Runtime started");
    }

    void onUpdate(float deltaTime) override
    {
        m_fps = 1.0f / (deltaTime > 0.0f ? deltaTime : 1.0f);

        if (m_cameraCtrl) m_cameraCtrl->onUpdate(deltaTime);
    }

    void onRender() override
    {
#ifdef ENGINE_DEBUG
        ImGui::Begin("Debug");
        ImGui::Text("FPS: %.1f", m_fps);
        ImGui::Separator();
        ImGui::Text("Resolution: %d * %d", getConfig().width, getConfig().height);
        ImGui::End();
#endif

    }

    void onShutdown() override
    {
        LOG_INFO("FaluEngine Runtime shutdown");
    }

private:
    float m_fps = 0.0f;
    std::unique_ptr<FaluEngine::CameraController> m_cameraCtrl;
};

int main() {
    RuntimeApp app;
    return app.run();
}
