#pragma once
#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <vector>

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

class PluginManager {
public:
    ~PluginManager() { unloadAll(); }

    bool load(const std::string& dllPath);
    void unload(const std::string& dllPath);
    void unloadAll();
    void updateAll(float deltaTime);

    [[nodiscard]] IPlugin* getPlugin(const std::string& dllPath) const;

private:
    struct Entry {
        DllHandle       handle  = nullptr;
        IPlugin*        plugin  = nullptr;
        DestroyPluginFn destroy = nullptr;
    };
    std::unordered_map<std::string, Entry> m_plugins;
};

} // namespace FaluEngine
