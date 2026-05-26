#include "AssetManager.h"
#include "core/Logger.h"

namespace FaluEngine {

void AssetManager::unload(const std::string& path) {
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        m_cache.erase(it);
        LOG_INFO("Asset unloaded: {}", path);
    }
}

void AssetManager::unloadAll() {
    m_cache.clear();
    LOG_INFO("All assets unloaded");
}

bool AssetManager::isLoaded(const std::string& path) const {
    return m_cache.count(path) > 0;
}

} // namespace FaluEngine
