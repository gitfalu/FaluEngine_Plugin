#include "plugin/IPlugin.h"
#include <cstdio>

// ── 物理プラグインの実装例 ─────────────────────────────────────────────────
class PhysicsPlugin final : public FaluEngine::IPlugin {
public:
    const char* getName()    const noexcept override { return "PhysicsPlugin"; }
    const char* getVersion() const noexcept override { return "0.1.0"; }

    bool onLoad() override {
        std::puts("[PhysicsPlugin] Loaded");
        // Bullet Physics / Jolt の初期化などをここで行う
        return true;
    }

    void onUnload() override {
        std::puts("[PhysicsPlugin] Unloaded");
    }

    void onUpdate(float deltaTime) override {
        // 物理シミュレーションのステップ処理
        (void)deltaTime;
    }
};

// ── DLL エクスポート ──────────────────────────────────────────────────────
ENGINE_PLUGIN_EXPORT FaluEngine::IPlugin* createPlugin()  { return new PhysicsPlugin(); }
ENGINE_PLUGIN_EXPORT void destroyPlugin(FaluEngine::IPlugin* p) { delete p; }
