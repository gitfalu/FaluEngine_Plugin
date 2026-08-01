#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <typeindex>
#include "core/Logger.h"
#include "core/FileWatcher.h"

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
        auto& typeCache = m_cache[std::type_index(typeid(T))];
        auto it = typeCache.find(path);
        if (it != typeCache.end()) 
            return std::static_pointer_cast<T>(it->second); 

        auto loaderIt = m_loaders.find(std::type_index(typeid(T)));
        if(loaderIt == m_loaders.end()) {
            LOG_ERROR("AssetManager : load asset failed {}", path.c_str());
            return nullptr;
        }

        auto asset = loaderIt->second(path);
        if (!asset) 
        {
            LOG_ERROR("AssetManager : asset is not found {}", path.c_str());
            return nullptr;
        }
        asset->path = path;
        asset->loaded = true;
        typeCache[path] = asset;

        watchForReload<T>(path);

        return std::static_pointer_cast<T>(asset);
    }

    void unload(const std::string& path);
    void unloadAll();
    void poll() { m_watcher.poll(); }

    [[nodiscard]] bool isLoaded(const std::string& path) const;
    [[nodiscard]] std::size_t getCacheSize() const noexcept {
        return m_cache.size();
    }

private:
    AssetManager()  = default;
    ~AssetManager() = default;

    template<typename T>
    void watchForReload(const std::string& path)
    {
        m_watcher.watch(path, [this, path]() {
            auto key = std::type_index(typeid(T));
            auto loaderIt = m_loaders.find(key);
            if (loaderIt == m_loaders.end()) return;

            auto reloaded = loaderIt->second(path);
            if (!reloaded) return;

            auto& typeCache = m_cache[key];
            auto it = typeCache.find(path);
            if (it != typeCache.end())
                *std::static_pointer_cast<T>(it->second) = *std::static_pointer_cast<T>(reloaded);
            });
    }

private:
    std::unordered_map<std::type_index,
        std::unordered_map<std::string, std::shared_ptr<Asset>>> m_cache; // 型ごとに分離
    std::unordered_map<std::type_index, AssetLoaderFn> m_loaders;
    FileWatcher m_watcher; // ホットリロード用
};

} // namespace FaluEngine
