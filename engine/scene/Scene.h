#pragma once
#include <entt/entt.hpp>
#include <string>
#include <functional>
#include <glm/glm.hpp>

namespace FaluEngine {

    class DX11Renderer;
class Entity;

class Scene {
public:
    explicit Scene(std::string name = "Untitled");
    ~Scene();

    [[nodiscard]] Entity createEntity(const std::string& name = "Entity");
    void destroyEntity(Entity entity);

    // 指定コンポーネントを持つ全エンティティに関数を適用する
    template<typename... Components,typename Func>
    void each(Func&& func) {
        auto view = m_registry.view<Components...>();
        for (auto entityHandle : view) {
            Entity e(entityHandle, this);
            func(e, view.template get<Components>(entityHandle)...);
        }
    }

    virtual void onEnter() {}
    virtual void onExit() {}
    void onUpdate(float deltaTime);
    void onRender(bool useOwnCamera = true);

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] entt::registry& registry() noexcept { return m_registry; }
    [[nodiscard]] uint32_t entityCount() const noexcept {
        return static_cast<uint32_t>(m_registry.storage<entt::entity>()->size());
    }

private:
    void updateWorldMatrices();
    void renderShadowPass(class DX11Renderer* renderer);
    void collectLights(class DX11Renderer* renderer, bool useOwnCamera);
    void renderMeshes(class DX11Renderer* renderer);
    void renderSky(class DX11Renderer* renderer);
    void renderUI(class DX11Renderer* renderer);
    void updateUILayout(DX11Renderer* renderer);
    void updateUIInteraction();
    void computeRectTransform(entt::entity entity,
        const glm::vec2& parentPos, const glm::vec2& parentSize);
private:
    std::string    m_name;
    entt::registry m_registry;
};

} // namespace FaluEngine
