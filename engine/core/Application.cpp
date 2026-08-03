#include "Application.h"
#include "Logger.h"
#include "platform/Window.h"
#include "renderer/IRenderer.h"
#include "scene/SceneManager.h"
#include "Events.h"
#include "asset/loaders/MeshLoader.h"
#include "asset/loaders/TextureLoader.h"
#include "physics/PhysicsSystem.h"
#include "script/ScriptEngine.h"
#include "InputManager.h"
#include "PathResolver.h"
#include "asset/loaders/ShaderLoader.h"
#include "asset/loaders/MaterialLoader.h"

#ifdef _WIN32
    #include "platform/win32/Win32Platform.h"
    #include "renderer/dx11/DX11Renderer.h"
#endif // _WIN32

#include <chrono>
#include <algorithm>

namespace FaluEngine {
    Application* Application::s_instance = nullptr;

Application::Application(const AppConfig& config)
    : m_config(config)
{
    s_instance = this;
}

Application::~Application() = default;
//******************************************************
// ウィンドウ生成
//******************************************************
bool Application::initWindow() {
#ifdef _WIN32
    m_window = std::make_unique<Win32Window>();
#else
    LOG_ERROR("Unsupported platform");
    return false;
#endif

    Window::Desc desc;
    desc.title = m_config.title;
    desc.width = m_config.width;
    desc.height = m_config.height;

    if (!m_window->create(desc))
    {
        LOG_ERROR("Failed to create window");
        return false;
    }

    m_window->setCloseCallback([this]() {
        EventBus::get().publish(WindowCloseEvent{});
        m_running = false;
    });

    m_window->setResizeCallback([this](uint32_t w, uint32_t h) {
        if (m_renderer) m_renderer->onResize(w, h);
        EventBus::get().publish(WindowResizeEvent{ w,h });
    });

    return true;
}

//**************************************************************
// レンダラー初期化
//**************************************************************
bool Application::initRenderer() {
#ifdef _WIN32
    m_renderer = std::make_unique<DX11Renderer>();
#else
    LOG_ERROR("Unsupported platform");
    return false;
#endif
    auto* renderer = static_cast<DX11Renderer*>(m_renderer.get());

    if (!m_renderer->init(m_window->getNativeHandle(), m_config.width, m_config.height)) {
        LOG_ERROR("Failed to initialize renderer");
        return false;
    }

    HWND hwnd = static_cast<HWND>(m_window->getNativeHandle());
    if (!m_imguiLayer.init(hwnd, renderer->getDevice(), renderer->getContext())) {
        return false;
    }
    
    registerMeshLoader(renderer->getDevice());
    registerTextureLoader(renderer->getDevice(), renderer->getContext());
    registerShaderLoader(renderer->getDevice());
    registerMaterialLoader();
    LOG_INFO("AssetManager loader registered");

    return true;
}

int Application::run() {
    Logger::init();
    PathResolver::Init();
    LOG_INFO("FaluEngine starting — {} x {}", m_config.width, m_config.height);

    if (!initWindow()) return -1;
    if (!initRenderer()) return -1;
    if (!PhysicsSystem::get().init())
    {
        LOG_ERROR("Failed to initialize PhysicsSystem");
        return -1;
    }
    EventBus::get().subscribe<SceneChangeEvent>([](const SceneChangeEvent& e) {
        auto* scene = SceneManager::get().getActive();
        if (scene) ScriptEngine::get().registerBindings(*scene);
        });
    m_running = true;
    onInit();

    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<float>;
    auto prev = Clock::now();

    while (m_running) {
        if (!m_window->pollEvents()) { m_running = false; break; }
        InputManager::get().update();
        
        auto now = Clock::now();
        float dt = (std::min)(
            std::chrono::duration_cast<Duration>(now - prev).count(), 0.25f);
        prev = now;
        
        AssetManager::get().poll();// Asset更新
        onUpdate(dt);//　フレーム更新
        SceneManager::get().onUpdate(dt); // シーン更新

        m_renderer->beginFrame(); // 描画更新
        m_imguiLayer.begin();
        SceneManager::get().onRender();
        onRender();
        m_imguiLayer.end();
        m_renderer->endFrame();

    }

    onShutdown();
    PhysicsSystem::get().shutdown();
    m_imguiLayer.shutdown();

    m_renderer.reset();
    m_window.reset();

    ScriptEngine::get().shutdown();

    LOG_INFO("FaluEngine shutdown");
    return 0;
}

} // namespace FaluEngine
