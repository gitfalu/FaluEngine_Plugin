#pragma once
#include "FaluEngine/IPlugin.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <FaluEngine/EngineExport.h>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <Windows.h>
    using DllHandle = HMODULE;
#else
    #include <dlfcn.h>
    using DllHandle = void*;
#endif

namespace FaluEngine {

class FALU_ENGINE_API PluginManager {
public:
    static PluginManager& get();

    ~PluginManager() { unloadAll(); }

    bool load(const std::string& dllPath);
    void unload(const std::string& dllPath);
    void unloadAll();
    void updateAll(float deltaTime);

    [[nodiscard]] IPlugin* getPlugin(const std::string& dllPath) const;
    [[nodiscard]] bool isLoaded(const std::string& dllPath) const
    {
        return m_plugins.count(dllPath) > 0;
    }

    bool reload(const std::string& dllPath)
    {
        if (isLoaded(dllPath))
            unload(dllPath);
        return load(dllPath);
    }

private:
    struct Entry {
        DllHandle       handle  = nullptr;
        IPlugin*        plugin  = nullptr;
        DestroyPluginFn destroy = nullptr;
    };
    std::unordered_map<std::string, Entry> m_plugins;
};

} // namespace FaluEngine
