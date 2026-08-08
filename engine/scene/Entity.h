#pragma once
#include <entt/entt.hpp>

namespace FaluEngine {

class Scene;

class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene)
        : m_handle(handle), m_scene(scene) {}

    // テンプレートメソッドはすべてヘッダーに実装する（LNK2019対策）
    template<typename T, typename... Args>
    T& addComponent(Args&&... args) {
        return getRegistry().emplace<T>(m_handle, std::forward<Args>(args)...);
    }

    template<typename T>
    [[nodiscard]] T& getComponent() {
        return getRegistry().get<T>(m_handle);
    }

    template<typename T>
    [[nodiscard]] const T& getComponent() const {
        return getRegistry().get<T>(m_handle);
    }

    template<typename T>
    [[nodiscard]] bool hasComponent() const {
        return getRegistry().all_of<T>(m_handle);
    }

    template<typename T>
    void removeComponent() {
        getRegistry().remove<T>(m_handle);
    }

    //===== 親子関係 =====
    void setParent(Entity parent);
    void removeParent();
    void addChild(Entity child);
    void removeChild(Entity child);

    [[nodiscard]] Entity getParent() const;
    [[nodiscard]] std::vector<Entity> getChildren() const;
    [[nodiscard]] bool hasParent() const;
    [[nodiscard]] bool hasChildren() const;
    [[nodiscard]] Scene* getScene() const noexcept { return m_scene; }

    [[nodiscard]] bool isValid() const noexcept {
        return m_handle != entt::null && m_scene != nullptr;
    }

    operator entt::entity() const noexcept { return m_handle; }

private:
    // Scene の定義より前に使うため前方参照でアクセスする実装を .cpp に分離
    entt::registry& getRegistry();
    const entt::registry& getRegistry() const;

    entt::entity m_handle = entt::null;
    Scene*       m_scene  = nullptr;
};

} // namespace FaluEngine