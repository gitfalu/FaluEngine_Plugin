#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>

#include <FaluEngine/EngineExport.h>
#include <FaluEngine/EventBus.h>
#include "FaluEngine/ImGuiLayer.h"

namespace FaluEngine {

class Window;
class IRenderer;
class SceneManager;

struct AppConfig {
    const std::wstring title  = L"FaluEngine";
    uint32_t       width  = 1280;
    uint32_t       height = 720;
    bool           vsync  = true;
};

// ゲーム / エディタはこのクラスを継承して各フックを実装する
class FALU_ENGINE_API Application {
public:
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    explicit Application(const AppConfig& config);
    virtual ~Application();

    [[nodiscard]] static Application& getInstance() noexcept { return *s_instance; }

    // メインループを開始し、終了コードを返す
    int run();

    // ── オーバーライドするフック ──────────────────────────────
    virtual void onInit()                  {}
    virtual void onUpdate(float deltaTime) { (void)deltaTime; }
    virtual void onRender()                {}
    virtual void onShutdown()              {}

    [[nodiscard]] const AppConfig& getConfig() const noexcept { return m_config; }
    [[nodiscard]] IRenderer* getRenderer() const noexcept { return m_renderer.get(); }
    [[nodiscard]] ImGuiLayer& getImGuiLayer() noexcept { return m_imguiLayer; }
    [[nodiscard]] static EventBus& getEventBus() noexcept;
        [[nodiscard]] static SceneManager& getSceneManager() noexcept;
    [[nodiscard]] bool isRunning()             const noexcept { return m_running; }
    void quit() noexcept { m_running = false; }

    // 仮置き公開関数
    [[nodiscard]] glm::vec3 getCameraPosition() const;
    static void switchScene(const std::string& name);

protected:
    static Application* s_instance;

    AppConfig m_config;
    bool      m_running = false;

    std::unique_ptr<Window> m_window;
    std::unique_ptr<IRenderer> m_renderer;
    FaluEngine::ImGuiLayer m_imguiLayer;

private:
    bool initWindow();
    bool initRenderer();
};

} // namespace FaluEngine
