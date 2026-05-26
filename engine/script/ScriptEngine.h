#pragma once
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>

namespace FaluEngine {

class Scene;

// Lua スクリプトエンジン。
// エンジン API を Lua に公開し、ScriptComponent の .lua ファイルを実行する。
class ScriptEngine {
public:
    static ScriptEngine& get()
    {
        static ScriptEngine instance;
        return instance;
    }
    void shutdown();

    // エンジン API を Lua 空間に登録する（起動時に一度だけ呼ぶ）
    void registerBindings(Scene& scene);

    // .lua ファイルを実行する
    bool executeFile(const std::string& path);

    // Lua 文字列を直接実行する（デバッグ用）
    bool executeString(const std::string& code);

    [[nodiscard]] sol::state& getLua() noexcept { return m_lua; }

private:
    ScriptEngine();

    sol::state m_lua;
};

} // namespace FaluEngine
