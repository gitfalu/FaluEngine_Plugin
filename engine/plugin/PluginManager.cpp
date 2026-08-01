#include "PluginManager.h"
#include "core/Logger.h"

namespace FaluEngine {

bool PluginManager::load(const std::string& dllPath) {
#ifdef _WIN32
    DllHandle handle = LoadLibraryA(dllPath.c_str());
#else
    DllHandle handle = dlopen(dllPath.c_str(), RTLD_LAZY);
#endif
    if (!handle) {
        LOG_ERROR("Failed to load plugin DLL: {}", dllPath);
        return false;
    }

#ifdef _WIN32
    auto create  = reinterpret_cast<CreatePluginFn> (GetProcAddress(handle, "createPlugin"));
    auto destroy = reinterpret_cast<DestroyPluginFn>(GetProcAddress(handle, "destroyPlugin"));
#else
    auto create  = reinterpret_cast<CreatePluginFn> (dlsym(handle, "createPlugin"));
    auto destroy = reinterpret_cast<DestroyPluginFn>(dlsym(handle, "destroyPlugin"));
#endif

    if (!create || !destroy) {
        LOG_ERROR("Plugin {} missing createPlugin/destroyPlugin exports", dllPath);
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return false;
    }

    IPlugin* plugin = create();
    if (!plugin)
    {
        LOG_ERROR("Plugin {}::createPlugin() returned nullptr", dllPath);
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return false;
    }

    if (!plugin->onLoad()) {
        LOG_ERROR("Plugin {}::onLoad() failed", dllPath);
        destroy(plugin);
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return false;
    }

    m_plugins[dllPath] = { handle, plugin, destroy };
    LOG_INFO("Plugin loaded: {} v{}", plugin->getName(), plugin->getVersion());
    return true;
}

void PluginManager::unload(const std::string& dllPath) {
    auto it = m_plugins.find(dllPath);
    if (it == m_plugins.end()) return;

    auto& e = it->second;
    e.plugin->onUnload();
    e.destroy(e.plugin);
#ifdef _WIN32
    FreeLibrary(e.handle);
#else
    dlclose(e.handle);
#endif
    LOG_INFO("Plugin unloaded: {}", dllPath);
    m_plugins.erase(it);
}

void PluginManager::unloadAll() {
    // コピーしてから削除（イテレータ無効化対策）
    std::vector<std::string> keys;
    keys.reserve(m_plugins.size());
    for (auto& [k, _] : m_plugins) keys.push_back(k);
    for (auto& k : keys) unload(k);
}

void PluginManager::updateAll(float deltaTime) {
    for (auto& [_, e] : m_plugins)
        e.plugin->onUpdate(deltaTime);
}

IPlugin* PluginManager::getPlugin(const std::string& dllPath) const {
    auto it = m_plugins.find(dllPath);
    return it != m_plugins.end() ? it->second.plugin : nullptr;
}

} // namespace FaluEngine
