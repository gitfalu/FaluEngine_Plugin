#include "ScriptEngine.h"
#include "core/Logger.h"
#include "core/InputManager.h"
#include "core/PathResolver.h"
#include "scene/Scene.h"
#include "scene/Entity.h"
#include "scene/Component.h"
#include <glm/glm.hpp>

namespace FaluEngine {

ScriptEngine::ScriptEngine() {
    m_lua.open_libraries(
        sol::lib::base, sol::lib::math, sol::lib::table,
        sol::lib::string, sol::lib::io, sol::lib::os,
        sol::lib::coroutine
    );
    LOG_INFO("ScriptEngine (Lua) initialized");
}

void ScriptEngine::shutdown()
{
    m_lua = sol::state();
}

void ScriptEngine::registerBindings(Scene& scene) {
    // ── Logger ──────────────────────────────────────────────────────────
    m_lua.set_function("log_info",  [](const std::string& msg){ LOG_INFO("{}", msg); });
    m_lua.set_function("log_warn",  [](const std::string& msg){ LOG_WARN("{}", msg); });
    m_lua.set_function("log_error", [](const std::string& msg){ LOG_ERROR("{}", msg); });

    // ── glm::vec3 ────────────────────────────────────────────────────────
    m_lua.new_usertype<glm::vec3>("vec3",
        sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z,
        sol::meta_function::addition,       [](const glm::vec3& a, const glm::vec3& b) {return a + b; },
        sol::meta_function::subtraction,    [](const glm::vec3& a, const glm::vec3& b) {return a - b; },
        sol::meta_function::multiplication, [](const glm::vec3& a, float s) {return a * s; }
    );

    //======= TransformComponent ==================
    m_lua.new_usertype<TransformComponent>("Transform",
        "position" , &TransformComponent::position,
        "scale" , &TransformComponent::scale,
        "setRotationEuler" , &TransformComponent::setRotationEuler
    );
    //=============================================

    //======= Entity ==================
    m_lua.new_usertype<Entity>("Entity",
        "getTransform", [](Entity& e)->TransformComponent& {
            return e.getComponent<TransformComponent>();
        },
        "getName", [](Entity& e)->std::string {
            return e.getComponent<TagComponent>().name;
        },
        "isValid",&Entity::isValid
    );
    //=============================================

    //======= Input ==================
    auto inputTable = m_lua.create_named_table("Input");
    inputTable.set_function("isKeyDown", [](const std::string& key)->bool {
        if (key.size() == 1) {
            char c = std::toupper(key[0]);
            return InputManager::get().isKeyDown(static_cast<Key>(c));
        }

        if (key == "Space") return InputManager::get().isKeyDown(Key::Space);
        if (key == "Shift") return InputManager::get().isKeyDown(Key::Shift);
        if (key == "Escape") return InputManager::get().isKeyDown(Key::Escape);
        if (key == "Left") return InputManager::get().isKeyDown(Key::Left);
        if (key == "Right") return InputManager::get().isKeyDown(Key::Right);
        if (key == "Up") return InputManager::get().isKeyDown(Key::Up);
        if (key == "Down") return InputManager::get().isKeyDown(Key::Down);
        return false;
    });
    inputTable.set_function("isKeyPressed", [](const std::string& key)->bool {
        if (key.size() == 1) {
            char c = std::toupper(key[0]);
            return InputManager::get().isKeyReleased(static_cast<Key>(c));
        }
        return false;
    });
    inputTable.set_function("getMouseDelta", []()-> glm::vec3{
        auto delta = InputManager::get().getMouseDelta();
        return { delta.x,delta.y ,0.0f};
    });
    inputTable.set_function("isMouseButtonDown", [](int btn)->bool {
        return InputManager::get().isMouseButtonDown(static_cast<MouseButton>(btn));
        });
    //=============================================
    //======= Scene ==================
    m_lua.new_usertype<Scene>("Scene",
        "createEntity", &Scene::createEntity,
        "entityCount", &Scene::entityCount,
        "getName", &Scene::getName
    );
    m_lua.set("scene", &scene);
    //=============================================
    //======= PathResolver ==================
    m_lua.set_function("resolverPath", [](const std::string& path)->std::string {
        return PathResolver::resolveStr(path);
        });

    //=============================================
}

bool ScriptEngine::executeFile(const std::string& path) {
    auto result = m_lua.safe_script_file(path, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERROR("Lua error in {}: {}", path, err.what());
        return false;
    }
    return true;
}

bool ScriptEngine::executeString(const std::string& code) {
    auto result = m_lua.safe_script(code, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERROR("Lua error: {}", err.what());
        return false;
    }
    return true;
}

} // namespace FaluEngine
