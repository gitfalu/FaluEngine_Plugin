#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <typeindex>
#include "core/Logger.h"

namespace FaluEngine {

// アセットの基底クラス
struct Asset {
    virtual ~Asset() = default;
    std::string path;
    bool loaded = false;
};

using AssetLoaderFn = std::function<std::shared_ptr<Asset>(const std::string&)>;

// 型安全なアセットキャッシュ。
// ローダーを差し替えられるよう、ロード処理は各 Loader クラスに委譲する。
class AssetManager {
public:
    static AssetManager& get() {
        static AssetManager instance;
        return instance;
    }

    template<typename T>
    void registerLoader(std::function<std::shared_ptr<T>(const std::string&)> loaderFn) {
        auto key = std::type_index(typeid(T));
        m_loaders[key] = [fn = std::move(loaderFn)](const std::string& path)
            ->std::shared_ptr<Asset> {
            return fn(path);
            };
    }

    template<typename T>
    std::shared_ptr<T> load(const std::string& path) {
        auto it = m_cache.find(path);
        if (it != m_cache.end())
            return std::static_pointer_cast<T>(it->second);

        auto key = std::type_index(typeid(T));
        auto loaderIt = m_loaders.find(key);
        if (loaderIt == m_loaders.end()) {
            LOG_ERROR("AssetManager: no loader registered for type '{}'", typeid(T).name());
            return nullptr;
        }

        auto asset = loaderIt->second(path);
        if (!asset) {
            LOG_ERROR("AssetManager: failed to load '{}'", path);
            return nullptr;
        }

        asset->path = path;
        asset->loaded = true;
        m_cache[path] = asset;
        LOG_INFO("AssetManager: loaded '{}'", path);

        return std::static_pointer_cast<T>(asset);
    }

    void unload(const std::string& path);
    void unloadAll();

    [[nodiscard]] bool isLoaded(const std::string& path) const;
    [[nodiscard]] std::size_t getCacheSize() const noexcept {
        return m_cache.size();
    }

private:
    AssetManager()  = default;
    ~AssetManager() = default;

    std::unordered_map<std::string, std::shared_ptr<Asset>> m_cache;
    std::unordered_map<std::type_index, AssetLoaderFn> m_loaders;
};

} // namespace FaluEngine
