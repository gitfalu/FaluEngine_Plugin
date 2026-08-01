#include "AssetManager.h"
#include "core/Logger.h"

namespace FaluEngine {

void AssetManager::unload(const std::string& path) {

    for (auto& [type, typeCache] : m_cache)
    {
        auto it = typeCache.find(path);
        if (it != typeCache.end()) { typeCache.erase(it); return; }
    }
}

void AssetManager::unloadAll() {
    m_cache.clear();
    LOG_INFO("All assets unloaded");
}

bool AssetManager::isLoaded(const std::string& path) const {
    for (auto& [type, typeCache] : m_cache)
    {
        if (typeCache.count(path)) return true;
    }
    return false;
}

} // namespace FaluEngine
