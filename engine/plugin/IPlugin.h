#pragma once
#include <string>
#include <memory>

namespace FaluEngine {

// ── プラグインインターフェース ────────────────────────────────────────────
// 全てのプラグインはこの純粋仮想クラスを実装する。
// DLL エクスポート関数 createPlugin() / destroyPlugin() を必ず定義すること。
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // プラグインのメタ情報
    [[nodiscard]] virtual const char* getName()    const noexcept = 0;
    [[nodiscard]] virtual const char* getVersion() const noexcept = 0;

    // ライフサイクル
    virtual bool onLoad()   = 0;   // DLLロード後に呼ばれる
    virtual void onUnload() = 0;   // DLLアンロード前に呼ばれる
    virtual void onUpdate(float deltaTime) = 0;
};

// ── DLL エクスポート規約 ──────────────────────────────────────────────────
// 各プラグイン DLL はこの2関数を必ずエクスポートする。
// PluginManager はこの関数ポインタ経由で IPlugin インスタンスを生成・破棄する。
using CreatePluginFn  = IPlugin*(*)();
using DestroyPluginFn = void(*)(IPlugin*);

} // namespace FaluEngine

// DLL エクスポートマクロ（プラグイン実装側で使用）
#ifdef _WIN32
  #define ENGINE_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
  #define ENGINE_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif
